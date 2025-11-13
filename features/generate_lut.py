#!/usr/bin/env python3
"""
PRODUCTION LOOKUP TABLE GENERATOR for RCWS Fire Control System

This script generates ballistic lookup tables in multiple formats:
1. CSV (for analysis/validation)
2. C++ header file (for embedded systems)
3. JSON (for Qt/QML integration)
4. Binary format (for high-speed loading)

Usage:
    python3 generate_lut.py --ammo m2ball --format all --output ./ballistic_tables/
"""

import math
import numpy as np
import json
import struct
from typing import Tuple, Dict, List
from dataclasses import dataclass, asdict
from pathlib import Path
import argparse

# ============================================================================
# BALLISTIC CORE (Same validated physics from previous code)
# ============================================================================

G = 9.80665
RHO_0 = 1.225
SCALE_HEIGHT = 8500.0

G1_DRAG_TABLE = np.array([
    [0.00, 0.2629], [0.05, 0.2558], [0.10, 0.2487], [0.15, 0.2413],
    [0.20, 0.2344], [0.25, 0.2278], [0.30, 0.2214], [0.35, 0.2155],
    [0.40, 0.2104], [0.45, 0.2061], [0.50, 0.2032], [0.55, 0.2020],
    [0.60, 0.2034], [0.70, 0.2165], [0.725, 0.2230], [0.75, 0.2313],
    [0.775, 0.2417], [0.80, 0.2546], [0.825, 0.2706], [0.85, 0.2901],
    [0.875, 0.3136], [0.90, 0.3415], [0.925, 0.3734], [0.95, 0.4084],
    [0.975, 0.4448], [1.0, 0.4805], [1.025, 0.5136], [1.05, 0.5427],
    [1.075, 0.5677], [1.10, 0.5883], [1.125, 0.6053], [1.15, 0.6191],
    [1.20, 0.6393], [1.25, 0.6518], [1.30, 0.6589], [1.35, 0.6621],
    [1.40, 0.6625], [1.45, 0.6607], [1.50, 0.6573], [1.55, 0.6528],
    [1.60, 0.6474], [1.65, 0.6413], [1.70, 0.6347], [1.75, 0.6280],
    [1.80, 0.6210], [1.85, 0.6141], [1.90, 0.6072], [1.95, 0.6003],
    [2.00, 0.5934], [2.05, 0.5867], [2.10, 0.5804], [2.15, 0.5743],
    [2.20, 0.5685], [2.25, 0.5630], [2.30, 0.5577], [2.35, 0.5527],
    [2.40, 0.5481], [2.45, 0.5438], [2.50, 0.5397], [2.60, 0.5325],
    [2.70, 0.5264], [2.80, 0.5211], [2.90, 0.5168], [3.00, 0.5133],
    [3.10, 0.5105], [3.20, 0.5084], [3.30, 0.5067], [3.40, 0.5054],
    [3.50, 0.5040], [3.60, 0.5030], [3.70, 0.5022], [3.80, 0.5016],
    [3.90, 0.5010], [4.00, 0.5006], [4.20, 0.4998], [4.40, 0.4995],
    [5.00, 0.4995],
])

def get_g1_drag_coefficient(mach: float) -> float:
    if mach <= G1_DRAG_TABLE[0, 0]: return G1_DRAG_TABLE[0, 1]
    if mach >= G1_DRAG_TABLE[-1, 0]: return G1_DRAG_TABLE[-1, 1]
    return float(np.interp(mach, G1_DRAG_TABLE[:, 0], G1_DRAG_TABLE[:, 1]))

@dataclass
class AmmunitionData:
    name: str
    diameter_m: float
    mass_kg: float
    ballistic_coefficient_g1: float
    muzzle_velocity_ms: float

    @property
    def reference_area_m2(self) -> float:
        return math.pi * (self.diameter_m / 2) ** 2
    
    @property
    def sectional_density_imperial(self) -> float:
        mass_lb = self.mass_kg * 2.20462
        diameter_in = self.diameter_m * 39.3701
        return mass_lb / (diameter_in ** 2)
    
    @property
    def form_factor(self) -> float:
        if self.ballistic_coefficient_g1 <= 0: return 1.0
        return self.sectional_density_imperial / self.ballistic_coefficient_g1

# ============================================================================
# AMMUNITION DATABASE
# ============================================================================

AMMUNITION_DATABASE = {
    'm2ball': AmmunitionData(
        name="M2 Ball",
        diameter_m=0.012954,
        mass_kg=0.04596,
        ballistic_coefficient_g1=0.62,
        muzzle_velocity_ms=887.0
    ),
    'm33ball': AmmunitionData(
        name="M33 Ball",
        diameter_m=0.012954,
        mass_kg=0.04285,  # 661 grains
        ballistic_coefficient_g1=0.670,
        muzzle_velocity_ms=887.0
    ),
    'm8api': AmmunitionData(
        name="M8 API",
        diameter_m=0.012954,
        mass_kg=0.04337,  # 669 grains
        ballistic_coefficient_g1=0.650,
        muzzle_velocity_ms=887.0
    ),
}

@dataclass
class Environment:
    temperature_celsius: float = 15.0

# ============================================================================
# TRAJECTORY SIMULATOR (Optimized for table generation)
# ============================================================================

class Projectile3DOF_G1:
    def __init__(self, ammo: AmmunitionData, env: Environment):
        self.ammo = ammo
        self.env = env
    
    def get_air_density(self, alt: float) -> float:
        return RHO_0 * math.exp(-alt / SCALE_HEIGHT)
    
    def get_speed_of_sound(self, alt: float) -> float:
        temp_k = max(1.0, (self.env.temperature_celsius - 0.0065 * alt) + 273.15)
        return 331.3 * math.sqrt(temp_k / 273.15)
    
    def derivatives(self, state: np.ndarray, t: float) -> np.ndarray:
        x, y, z, vx, vy, vz = state
        rho = self.get_air_density(z)
        sos = self.get_speed_of_sound(z)
        vel = np.array([vx, vy, vz])
        v_mag = np.linalg.norm(vel)
        
        if v_mag < 1e-6:
            return np.array([vx, vy, vz, 0.0, 0.0, -G])
        
        accel_gravity = np.array([0.0, 0.0, -G])
        mach = v_mag / sos
        cd_g1 = get_g1_drag_coefficient(mach)
        cd_actual = cd_g1 * self.ammo.form_factor
        drag_force = 0.5 * rho * self.ammo.reference_area_m2 * v_mag**2 * cd_actual
        accel_drag = -(drag_force / self.ammo.mass_kg) * (vel / v_mag)
        ax, ay, az = accel_gravity + accel_drag
        return np.array([vx, vy, vz, ax, ay, az])
    
    def rk4_step(self, state: np.ndarray, t: float, dt: float) -> np.ndarray:
        k1 = self.derivatives(state, t)
        k2 = self.derivatives(state + 0.5 * dt * k1, t + 0.5 * dt)
        k3 = self.derivatives(state + 0.5 * dt * k2, t + 0.5 * dt)
        k4 = self.derivatives(state + dt * k3, t + dt)
        return state + (dt / 6.0) * (k1 + 2*k2 + 2*k3 + k4)
    
    def simulate_trajectory(self, elevation_deg: float, dt: float = 0.01,
                          max_time: float = 150.0) -> Tuple[np.ndarray, np.ndarray]:
        el_rad = math.radians(elevation_deg)
        mv = self.ammo.muzzle_velocity_ms
        
        state = np.array([
            0.0, 0.0, 0.0,
            0.0,
            mv * math.cos(el_rad),
            mv * math.sin(el_rad)
        ])
        
        trajectory = [np.concatenate(([0.0], state))]
        t = 0.0
        
        while t < max_time and state[2] >= -0.1:
            state = self.rk4_step(state, t, dt)
            t += dt
            trajectory.append(np.concatenate(([t], state)))
        
        trajectory = np.array(trajectory)
        impact_data = np.array([
            trajectory[-1, 2],  # range (y)
            trajectory[-1, 0],  # time
            np.linalg.norm(trajectory[-1, 4:7])  # velocity
        ])
        
        return trajectory, impact_data

def find_elevation_for_range(projectile: Projectile3DOF_G1, target_range_m: float) -> Dict:
    angle_low, angle_high = 0.0, 45.0
    _, impact_high = projectile.simulate_trajectory(angle_high)
    if impact_high[0] < target_range_m:
        raise ValueError(f"Target range {target_range_m}m exceeds maximum range")
    
    for _ in range(50):
        angle_mid = (angle_low + angle_high) / 2.0
        _, impact_mid = projectile.simulate_trajectory(angle_mid)
        
        if abs(impact_mid[0] - target_range_m) < 0.1:
            break
        
        if impact_mid[0] < target_range_m:
            angle_low = angle_mid
        else:
            angle_high = angle_mid
    
    return {
        'elevation_deg': angle_mid,
        'elevation_mils': angle_mid * (6400 / 360.0),
        'time_of_flight_s': impact_mid[1],
        'impact_velocity_ms': impact_mid[2]
    }

# ============================================================================
# LOOKUP TABLE GENERATOR
# ============================================================================

class LookupTableGenerator:
    def __init__(self, ammo: AmmunitionData, range_start: int = 100,
                 range_end: int = 2000, range_step: int = 50):
        self.ammo = ammo
        self.range_start = range_start
        self.range_end = range_end
        self.range_step = range_step
        self.projectile = Projectile3DOF_G1(ammo, Environment())
    
    def generate(self) -> List[Dict]:
        """Generate complete lookup table."""
        table = []
        
        print(f"\nGenerating lookup table for {self.ammo.name}...")
        print(f"Range: {self.range_start}m to {self.range_end}m, step {self.range_step}m")
        print(f"Progress: ", end='', flush=True)
        
        ranges = range(self.range_start, self.range_end + 1, self.range_step)
        total = len(list(ranges))
        
        for i, r in enumerate(range(self.range_start, self.range_end + 1, self.range_step)):
            try:
                solution = find_elevation_for_range(self.projectile, float(r))
                table.append({
                    'range_m': r,
                    'elevation_deg': solution['elevation_deg'],
                    'elevation_mils': solution['elevation_mils'],
                    'tof_s': solution['time_of_flight_s'],
                    'impact_velocity_ms': solution['impact_velocity_ms']
                })
                
                # Progress indicator
                if (i + 1) % 5 == 0:
                    print(f"{int((i+1)/total*100)}% ", end='', flush=True)
                    
            except ValueError as e:
                print(f"\nWarning: Could not compute {r}m - {e}")
        
        print("✓ Complete\n")
        return table
    
    def save_csv(self, table: List[Dict], filepath: Path):
        """Save as CSV file."""
        with open(filepath, 'w') as f:
            f.write("# Ballistic Lookup Table\n")
            f.write(f"# Ammunition: {self.ammo.name}\n")
            f.write(f"# BC(G1): {self.ammo.ballistic_coefficient_g1}\n")
            f.write(f"# Muzzle Velocity: {self.ammo.muzzle_velocity_ms} m/s\n")
            f.write("#\n")
            f.write("Range(m),Elevation(deg),Elevation(mils),TOF(s),ImpactVel(m/s)\n")
            
            for entry in table:
                f.write(f"{entry['range_m']},{entry['elevation_deg']:.6f},"
                       f"{entry['elevation_mils']:.4f},{entry['tof_s']:.6f},"
                       f"{entry['impact_velocity_ms']:.2f}\n")
        
        print(f"✓ Saved CSV: {filepath}")
    
    def save_cpp_header(self, table: List[Dict], filepath: Path):
        """Save as C++ header file for embedded systems."""
        with open(filepath, 'w') as f:
            f.write("/*\n")
            f.write(" * AUTO-GENERATED BALLISTIC LOOKUP TABLE\n")
            f.write(f" * Ammunition: {self.ammo.name}\n")
            f.write(f" * BC(G1): {self.ammo.ballistic_coefficient_g1}\n")
            f.write(f" * Muzzle Velocity: {self.ammo.muzzle_velocity_ms} m/s\n")
            f.write(" * DO NOT EDIT MANUALLY\n")
            f.write(" */\n\n")
            f.write("#ifndef BALLISTIC_LUT_H\n")
            f.write("#define BALLISTIC_LUT_H\n\n")
            f.write("#include <cstdint>\n\n")
            
            # Table structure
            f.write("struct BallisticEntry {\n")
            f.write("    uint16_t range_m;           // Range in meters\n")
            f.write("    float elevation_mils;       // Elevation in mils\n")
            f.write("    float tof_s;                // Time of flight in seconds\n")
            f.write("    float impact_velocity_ms;   // Impact velocity in m/s\n")
            f.write("};\n\n")
            
            # Table data
            ammo_name_safe = self.ammo.name.replace(" ", "_").upper()
            f.write(f"constexpr uint32_t LUT_{ammo_name_safe}_SIZE = {len(table)};\n\n")
            f.write(f"constexpr BallisticEntry LUT_{ammo_name_safe}[LUT_{ammo_name_safe}_SIZE] = {{\n")
            
            for entry in table:
                f.write(f"    {{{entry['range_m']:>4}, "
                       f"{entry['elevation_mils']:>8.4f}f, "
                       f"{entry['tof_s']:>8.6f}f, "
                       f"{entry['impact_velocity_ms']:>7.2f}f}},\n")
            
            f.write("};\n\n")
            f.write("#endif // BALLISTIC_LUT_H\n")
        
        print(f"✓ Saved C++ header: {filepath}")
    
    def save_json(self, table: List[Dict], filepath: Path):
        """Save as JSON for Qt/QML integration."""
        output = {
            'ammunition': {
                'name': self.ammo.name,
                'diameter_mm': self.ammo.diameter_m * 1000,
                'mass_grams': self.ammo.mass_kg * 1000,
                'bc_g1': self.ammo.ballistic_coefficient_g1,
                'muzzle_velocity_ms': self.ammo.muzzle_velocity_ms
            },
            'generation_parameters': {
                'range_start_m': self.range_start,
                'range_end_m': self.range_end,
                'range_step_m': self.range_step
            },
            'lookup_table': table
        }
        
        with open(filepath, 'w') as f:
            json.dump(output, f, indent=2)
        
        print(f"✓ Saved JSON: {filepath}")
    
    def save_binary(self, table: List[Dict], filepath: Path):
        """Save as binary format for high-speed loading."""
        with open(filepath, 'wb') as f:
            # Header
            f.write(b'BLUT')  # Magic number
            f.write(struct.pack('<I', 1))  # Version
            f.write(struct.pack('<I', len(table)))  # Number of entries
            
            # Ammunition metadata
            name_bytes = self.ammo.name.encode('utf-8')[:32].ljust(32, b'\0')
            f.write(name_bytes)
            f.write(struct.pack('<f', self.ammo.ballistic_coefficient_g1))
            f.write(struct.pack('<f', self.ammo.muzzle_velocity_ms))
            
            # Table entries
            for entry in table:
                f.write(struct.pack('<H', entry['range_m']))
                f.write(struct.pack('<f', entry['elevation_mils']))
                f.write(struct.pack('<f', entry['tof_s']))
                f.write(struct.pack('<f', entry['impact_velocity_ms']))
        
        print(f"✓ Saved binary: {filepath} ({filepath.stat().st_size} bytes)")

# ============================================================================
# MAIN EXECUTION
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Generate ballistic lookup tables for RCWS fire control'
    )
    parser.add_argument('--ammo', type=str, default='m2ball',
                       choices=list(AMMUNITION_DATABASE.keys()),
                       help='Ammunition type')
    parser.add_argument('--format', type=str, default='all',
                       choices=['csv', 'cpp', 'json', 'binary', 'all'],
                       help='Output format')
    parser.add_argument('--output', type=str, default='./ballistic_tables/',
                       help='Output directory')
    parser.add_argument('--range-start', type=int, default=100,
                       help='Start range (meters)')
    parser.add_argument('--range-end', type=int, default=2000,
                       help='End range (meters)')
    parser.add_argument('--range-step', type=int, default=50,
                       help='Range step (meters)')
    
    args = parser.parse_args()
    
    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Get ammunition
    ammo = AMMUNITION_DATABASE[args.ammo]
    
    print("=" * 80)
    print("BALLISTIC LOOKUP TABLE GENERATOR")
    print("=" * 80)
    print(f"\nAmmunition: {ammo.name}")
    print(f"  Bullet weight: {ammo.mass_kg * 15432.36:.0f} grains")
    print(f"  BC (G1): {ammo.ballistic_coefficient_g1}")
    print(f"  Muzzle velocity: {ammo.muzzle_velocity_ms:.0f} m/s")
    
    # Generate table
    generator = LookupTableGenerator(
        ammo,
        range_start=args.range_start,
        range_end=args.range_end,
        range_step=args.range_step
    )
    table = generator.generate()
    
    # Save in requested formats
    ammo_name_safe = ammo.name.replace(" ", "_").lower()
    
    if args.format in ['csv', 'all']:
        generator.save_csv(table, output_dir / f"{ammo_name_safe}.csv")
    
    if args.format in ['cpp', 'all']:
        generator.save_cpp_header(table, output_dir / f"{ammo_name_safe}.h")
    
    if args.format in ['json', 'all']:
        generator.save_json(table, output_dir / f"{ammo_name_safe}.json")
    
    if args.format in ['binary', 'all']:
        generator.save_binary(table, output_dir / f"{ammo_name_safe}.blut")
    
    print("\n" + "=" * 80)
    print("✓ GENERATION COMPLETE")
    print("=" * 80)
    print(f"\nGenerated {len(table)} entries")
    print(f"Output directory: {output_dir.absolute()}")
    print(f"\nTable summary:")
    print(f"  Min range: {table[0]['range_m']}m")
    print(f"  Max range: {table[-1]['range_m']}m")
    print(f"  Elevation range: {table[0]['elevation_mils']:.2f} to {table[-1]['elevation_mils']:.2f} mils")
    print(f"  TOF range: {table[0]['tof_s']:.3f}s to {table[-1]['tof_s']:.3f}s")

if __name__ == "__main__":
    main()
