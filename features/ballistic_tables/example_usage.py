#!/usr/bin/env python3
"""
SIMPLE EXAMPLE: Using Ballistic Lookup Tables in Practice

This shows the minimal code needed to:
1. Load a lookup table
2. Interpolate for any range
3. Apply environmental corrections
"""

import json
import math

# ============================================================================
# STEP 1: LOAD LOOKUP TABLE
# ============================================================================

def load_table(filepath):
    """Load ballistic table from JSON file."""
    with open(filepath, 'r') as f:
        data = json.load(f)
    
    table = data['lookup_table']
    
    # Convert to simple arrays for fast access
    ranges = [entry['range_m'] for entry in table]
    elevations = [entry['elevation_mils'] for entry in table]
    tofs = [entry['tof_s'] for entry in table]
    velocities = [entry['impact_velocity_ms'] for entry in table]
    
    return {
        'ranges': ranges,
        'elevations': elevations,
        'tofs': tofs,
        'velocities': velocities,
        'ammo_name': data['ammunition']['name']
    }

# ============================================================================
# STEP 2: INTERPOLATION
# ============================================================================

def find_bracket(target_range, ranges):
    """Binary search to find table bracket for target range."""
    if target_range <= ranges[0]:
        return 0
    if target_range >= ranges[-1]:
        return len(ranges) - 2
    
    left, right = 0, len(ranges) - 1
    
    while right - left > 1:
        mid = (left + right) // 2
        if ranges[mid] < target_range:
            left = mid
        else:
            right = mid
    
    return left

def interpolate(target_range, table):
    """Get firing solution for any range using linear interpolation."""
    idx = find_bracket(target_range, table['ranges'])
    
    # Get bracket values
    r1 = table['ranges'][idx]
    r2 = table['ranges'][idx + 1]
    e1 = table['elevations'][idx]
    e2 = table['elevations'][idx + 1]
    t1 = table['tofs'][idx]
    t2 = table['tofs'][idx + 1]
    v1 = table['velocities'][idx]
    v2 = table['velocities'][idx + 1]
    
    # Interpolation factor
    t = (target_range - r1) / (r2 - r1)
    
    # Interpolate all values
    return {
        'range_m': target_range,
        'elevation_mils': e1 + t * (e2 - e1),
        'tof_s': t1 + t * (t2 - t1),
        'impact_velocity_ms': v1 + t * (v2 - v1)
    }

# ============================================================================
# STEP 3: ENVIRONMENTAL CORRECTIONS
# ============================================================================

def apply_temperature_correction(elevation_mils, temp_celsius):
    """Correct elevation for non-standard temperature."""
    temp_corr = math.sqrt(288.15 / (temp_celsius + 273.15))
    return elevation_mils * temp_corr

def apply_altitude_correction(elevation_mils, altitude_m):
    """Correct elevation for altitude."""
    rho_corr = 1.0 / math.exp(-altitude_m / 8500.0)
    return elevation_mils * rho_corr

def calculate_wind_lead(range_m, tof_s, crosswind_ms):
    """Calculate wind lead angle in mils."""
    deflection_m = crosswind_ms * tof_s
    lead_mils = (deflection_m / range_m) * 1000.0
    return lead_mils

def get_complete_solution(target_range, table, temp_c=15.0, altitude_m=0.0, wind_ms=0.0):
    """
    Get complete firing solution with environmental corrections.
    
    Args:
        target_range: Target distance in meters
        table: Loaded lookup table
        temp_c: Temperature in Celsius (default 15°C)
        altitude_m: Altitude in meters (default 0m)
        wind_ms: Crosswind in m/s (default 0)
    
    Returns:
        dict with elevation_mils, azimuth_correction_mils, tof_s
    """
    # Get base solution from table
    solution = interpolate(target_range, table)
    
    # Apply corrections
    elevation = solution['elevation_mils']
    elevation = apply_temperature_correction(elevation, temp_c)
    elevation = apply_altitude_correction(elevation, altitude_m)
    
    # Wind correction
    wind_lead = calculate_wind_lead(target_range, solution['tof_s'], wind_ms)
    
    return {
        'elevation_mils': elevation,
        'azimuth_correction_mils': wind_lead,
        'tof_s': solution['tof_s'],
        'impact_velocity_ms': solution['impact_velocity_ms']
    }

# ============================================================================
# STEP 4: USAGE EXAMPLE
# ============================================================================

def main():
    print("="*80)
    print("BALLISTIC LOOKUP TABLE - USAGE EXAMPLE")
    print("="*80)
    
    # Load table
    print("\n1. Loading lookup table...")
    table = load_table('m2_ball.json')
    print(f"   ✓ Loaded: {table['ammo_name']}")
    print(f"   ✓ Entries: {len(table['ranges'])}")
    print(f"   ✓ Range: {table['ranges'][0]}m to {table['ranges'][-1]}m")
    
    # Example 1: Simple interpolation (standard conditions)
    print("\n2. Example: Standard conditions (15°C, sea level, no wind)")
    print("   " + "-"*70)
    
    test_ranges = [100, 567, 1000, 1234, 2000]
    for r in test_ranges:
        sol = interpolate(r, table)
        print(f"   Range: {sol['range_m']:>4}m → "
              f"Elev: {sol['elevation_mils']:>6.2f} mils, "
              f"TOF: {sol['tof_s']:>5.3f}s, "
              f"Vel: {sol['impact_velocity_ms']:>6.1f} m/s")
    
    # Example 2: With environmental corrections
    print("\n3. Example: Hot day with altitude and wind")
    print("   Conditions: 35°C, 800m altitude, 5 m/s crosswind")
    print("   " + "-"*70)
    
    for r in [500, 1000, 1500]:
        sol = get_complete_solution(r, table, temp_c=35.0, altitude_m=800.0, wind_ms=5.0)
        print(f"   Range: {r:>4}m → "
              f"Elev: {sol['elevation_mils']:>6.2f} mils, "
              f"Wind lead: {sol['azimuth_correction_mils']:>+5.2f} mils, "
              f"TOF: {sol['tof_s']:>5.3f}s")
    
    # Example 3: Real-time fire control simulation
    print("\n4. Real-time fire control simulation")
    print("   " + "-"*70)
    
    # Simulated target tracking
    targets = [
        {'range': 834, 'temp': 25, 'alt': 200, 'wind': 3},
        {'range': 1456, 'temp': 18, 'alt': 0, 'wind': -2},
        {'range': 622, 'temp': 30, 'alt': 500, 'wind': 7},
    ]
    
    for i, tgt in enumerate(targets, 1):
        sol = get_complete_solution(
            tgt['range'], table, 
            temp_c=tgt['temp'],
            altitude_m=tgt['alt'],
            wind_ms=tgt['wind']
        )
        print(f"   Target {i}: {tgt['range']}m")
        print(f"     Conditions: {tgt['temp']}°C, {tgt['alt']}m alt, {tgt['wind']:+}m/s wind")
        print(f"     FIRE COMMAND: Elevation {sol['elevation_mils']:.2f} mils, "
              f"Azimuth {sol['azimuth_correction_mils']:+.2f} mils")
        print(f"     Expected impact in {sol['tof_s']:.3f}s")
        print()
    
    print("="*80)
    print("✓ COMPLETE - Ready for RCWS integration")
    print("="*80)

if __name__ == "__main__":
    main()
