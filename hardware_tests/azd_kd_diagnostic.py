#!/usr/bin/env python3
"""
Oriental Motor AZD-KD Servo Driver Diagnostic Tool
Reads alarm history, position, encoder status, and monitors for position jumps.

Install: pip install minimalmodbus
"""

import time
from datetime import datetime
import minimalmodbus

# ============================================================================
# CONFIGURATION
# ============================================================================
CONFIG = {
    "port": "/dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BC046FABCD-if04",
    "baudrate": 230400,
    "parity": "N",
    "stopbits": 1,
    "bytesize": 8,
    "slave_id": 2,
    "timeout": 0.5,
}

# ============================================================================
# AZD-KD REGISTER MAP
# ============================================================================
REGISTERS = {
    # Position and speed (Read)
    "POSITION": 0x0000,              # 2 registers, 32-bit signed
    "SPEED": 0x0002,                 # 2 registers, 32-bit signed
    
    # Temperature (Read)
    "DRIVER_TEMP": 0x0080,           # 2 registers (x0.1 °C)
    "MOTOR_TEMP": 0x0082,            # 2 registers (x0.1 °C)
    
    # Alarm status (Read)
    "ALARM_STATUS": 0x0100,          # 2 registers
    "ALARM_HISTORY_1": 0x0102,       # 2 registers per entry, 10 entries
    
    # Maintenance commands (Write)
    "ALARM_RESET": 0x0180,           # Write 1 to reset
    "ALARM_HISTORY_CLEAR": 0x0184,   # Write 1 to clear
    "P_PRESET": 0x018A,              # Position preset execution
}

# ============================================================================
# ALARM CODE DEFINITIONS
# ============================================================================
ALARM_CODES = {
    0x10: "Excessive position deviation",
    0x20: "Overcurrent",
    0x21: "Main circuit overheat",
    0x22: "Overvoltage",
    0x23: "Main power supply OFF",
    0x25: "Undervoltage",
    0x26: "Motor overheat",
    0x28: "Sensor error",
    0x2A: "ABZO sensor communication error",
    0x30: "Overload",
    0x31: "Overspeed",
    0x33: "Absolute position error ⚠️ HOME POSITION DAMAGED",
    0x34: "Command pulse error",
    0x41: "EEPROM error",
    0x42: "Sensor error at power on",
    0x43: "Rotation error at power on",
    0x44: "Encoder EEPROM error ⚠️ ABZO DATA CORRUPTED",
    0x45: "Motor combination error",
    0x4A: "Return-to-home incomplete ⚠️ POSITION NOT SET",
    0x51: "Regeneration resistor overheat",
    0x53: "HWTO input circuit error",
    0x60: "±LS both sides active",
    0x61: "Reverse ±LS connection",
    0x62: "Return-to-home operation error",
    0x63: "No HOMES",
    0x64: "TIM, Z, SLIT signal error",
    0x66: "Hardware overtravel",
    0x67: "Software overtravel",
    0x68: "HWTO input detection",
    0x6A: "Return-to-home operation offset error",
    0x6D: "Mechanical overtravel",
    0x70: "Operation data error",
    0x71: "Electronic gear setting error",
    0x72: "Wrap setting error",
    0x81: "Network bus error",
    0x83: "Communication switch setting error",
    0x84: "RS-485 communication error",
    0x85: "RS-485 communication timeout",
    0x8E: "Network converter error",
    0xF0: "CPU error",
}

# Critical alarms that affect position
POSITION_AFFECTING_ALARMS = [0x33, 0x44, 0x4A, 0x42, 0x43]


class AZDKDDiagnostic:
    def __init__(self, config):
        self.config = config
        self.instrument = None
        
    def connect(self):
        """Establish Modbus RTU connection."""
        print(f"\n{'='*60}")
        print("CONNECTING TO AZD-KD SERVO DRIVER")
        print(f"{'='*60}")
        print(f"Port: {self.config['port']}")
        print(f"Baudrate: {self.config['baudrate']}")
        print(f"Slave ID: {self.config['slave_id']}")
        
        try:
            self.instrument = minimalmodbus.Instrument(
                self.config["port"],
                self.config["slave_id"]
            )
            self.instrument.serial.baudrate = self.config["baudrate"]
            self.instrument.serial.parity = minimalmodbus.serial.PARITY_NONE
            self.instrument.serial.stopbits = self.config["stopbits"]
            self.instrument.serial.bytesize = self.config["bytesize"]
            self.instrument.serial.timeout = self.config["timeout"]
            self.instrument.mode = minimalmodbus.MODE_RTU
            self.instrument.clear_buffers_before_each_transaction = True
            
            print("✅ Connected successfully!")
            return True
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Close connection."""
        if self.instrument:
            self.instrument.serial.close()
            print("Disconnected.")
    
    def read_registers(self, address, count):
        """Read holding registers and return raw values."""
        try:
            return self.instrument.read_registers(
                registeraddress=address,
                number_of_registers=count,
                functioncode=3  # Read Holding Registers
            )
        except Exception as e:
            print(f"  ❌ Exception reading register 0x{address:04X}: {e}")
            return None
    
    def read_32bit_signed(self, address):
        """Read 32-bit signed integer from 2 registers (Big Endian - High word first)."""
        regs = self.read_registers(address, 2)
        if regs is None:
            return None
        
        # Oriental Motor uses Big Endian: Register 0 = High word, Register 1 = Low word
        high_word = regs[0]
        low_word = regs[1]
        
        # Combine and handle signed
        value = (high_word << 16) | low_word
        if value >= 0x80000000:
            value -= 0x100000000
        
        return value, low_word, high_word
    
    def read_position(self):
        """Read current position."""
        result = self.read_32bit_signed(REGISTERS["POSITION"])
        if result is None:
            return None
        
        position, low, high = result
        return {
            "position_steps": position,
            "low_word": low,
            "high_word": high,
            "raw_hex": f"0x{high:04X}{low:04X}"
        }
    
    def read_speed(self):
        """Read current speed."""
        result = self.read_32bit_signed(REGISTERS["SPEED"])
        if result is None:
            return None
        return result[0]
    
    def read_temperatures(self):
        """Read driver and motor temperatures."""
        driver_regs = self.read_registers(REGISTERS["DRIVER_TEMP"], 2)
        motor_regs = self.read_registers(REGISTERS["MOTOR_TEMP"], 2)
        
        result = {
            "driver_temp": None,
            "motor_temp": None,
            "driver_raw": driver_regs,
            "motor_raw": motor_regs
        }
        
        if driver_regs:
            # Temperature is typically in lower register only, scaled by 0.1
            result["driver_temp"] = driver_regs[1] * 0.1
        
        if motor_regs:
            result["motor_temp"] = motor_regs[1] * 0.1
        
        return result
    
    def read_alarm_status(self):
        """Read current alarm status."""
        regs = self.read_registers(REGISTERS["ALARM_STATUS"], 2)
        if regs is None:
            return None
        
        # Big Endian: Register 0 = High word, Register 1 = Low word
        # But alarm codes are typically just in the low word
        alarm_code = regs[1]  # Usually just the low word contains the alarm code
        
        return {
            "code": alarm_code,
            "hex": f"0x{alarm_code:02X}",
            "description": ALARM_CODES.get(alarm_code, "No alarm" if alarm_code == 0 else f"Unknown (0x{alarm_code:04X})"),
            "affects_position": alarm_code in POSITION_AFFECTING_ALARMS,
            "raw_high": regs[0],
            "raw_low": regs[1]
        }
    
    def read_alarm_history(self):
        """Read alarm history (up to 10 entries)."""
        history = []
        
        # Alarm history starts at 0x0102, 2 registers per entry
        for i in range(10):
            address = REGISTERS["ALARM_HISTORY_1"] + (i * 2)
            regs = self.read_registers(address, 2)
            
            if regs is None:
                continue
            
            # Alarm code is typically in the low word (regs[1])
            alarm_code = regs[1]
            
            if alarm_code != 0 and alarm_code != 0xFFFF:
                history.append({
                    "index": i + 1,
                    "code": alarm_code,
                    "hex": f"0x{alarm_code:02X}",
                    "description": ALARM_CODES.get(alarm_code, f"Unknown alarm (0x{alarm_code:02X})"),
                    "affects_position": alarm_code in POSITION_AFFECTING_ALARMS
                })
        
        return history
    
    def steps_to_degrees(self, steps):
        """Convert motor steps to gimbal degrees."""
        gear_ratio = 174.0 / 34.0  # 5.117
        motor_step_deg = 0.009     # degrees per step
        deg_per_step = motor_step_deg / gear_ratio
        return steps * deg_per_step
    
    def print_separator(self, title):
        """Print section separator."""
        print(f"\n{'='*60}")
        print(f" {title}")
        print(f"{'='*60}")
    
    def run_full_diagnostic(self):
        """Run complete diagnostic."""
        if not self.connect():
            return
        
        try:
            # ============================================================
            # 1. CURRENT POSITION
            # ============================================================
            self.print_separator("CURRENT POSITION")
            pos = self.read_position()
            if pos:
                gimbal_deg = self.steps_to_degrees(pos["position_steps"])
                print(f"  Position (steps):  {pos['position_steps']:,}")
                print(f"  Position (gimbal): {gimbal_deg:.2f}°")
                print(f"  Low word:          0x{pos['low_word']:04X} ({pos['low_word']})")
                print(f"  High word:         0x{pos['high_word']:04X} ({pos['high_word']})")
                print(f"  Raw hex:           {pos['raw_hex']}")
                
                # Check if position seems reasonable
                if abs(pos["position_steps"]) > 10000000:
                    print("  ⚠️  WARNING: Position value seems abnormally large!")
            
            # ============================================================
            # 2. CURRENT SPEED
            # ============================================================
            self.print_separator("CURRENT SPEED")
            speed = self.read_speed()
            if speed is not None:
                print(f"  Speed: {speed} steps/s")
            
            # ============================================================
            # 3. TEMPERATURES
            # ============================================================
            self.print_separator("TEMPERATURES")
            temps = self.read_temperatures()
            if temps["driver_raw"]:
                print(f"  Driver raw: {temps['driver_raw']} → {temps['driver_temp']:.1f} °C")
            if temps["motor_raw"]:
                print(f"  Motor raw:  {temps['motor_raw']} → {temps['motor_temp']:.1f} °C")
            
            # ============================================================
            # 4. CURRENT ALARM STATUS
            # ============================================================
            self.print_separator("CURRENT ALARM STATUS")
            alarm = self.read_alarm_status()
            if alarm:
                print(f"  Raw registers: High=0x{alarm['raw_high']:04X} Low=0x{alarm['raw_low']:04X}")
                if alarm["code"] == 0:
                    print("  ✅ No active alarm")
                else:
                    print(f"  ❌ ACTIVE ALARM!")
                    print(f"     Code: {alarm['hex']}")
                    print(f"     Description: {alarm['description']}")
                    if alarm["affects_position"]:
                        print("     ⚠️  THIS ALARM AFFECTS POSITION DATA!")
            
            # ============================================================
            # 5. ALARM HISTORY (CRITICAL!)
            # ============================================================
            self.print_separator("ALARM HISTORY (Last 10 alarms)")
            history = self.read_alarm_history()
            
            if not history:
                print("  ✅ No alarms in history")
            else:
                position_alarms_found = False
                
                for entry in history:
                    status = "⚠️ POSITION AFFECTING!" if entry["affects_position"] else ""
                    print(f"  [{entry['index']:2d}] {entry['hex']}: {entry['description']} {status}")
                    
                    if entry["affects_position"]:
                        position_alarms_found = True
                
                if position_alarms_found:
                    print("\n  " + "!"*50)
                    print("  !!! POSITION-AFFECTING ALARMS FOUND IN HISTORY !!!")
                    print("  !!! This is likely the cause of your 40,000 step drift !!!")
                    print("  " + "!"*50)
            
            # ============================================================
            # 6. SUMMARY
            # ============================================================
            self.print_separator("DIAGNOSIS SUMMARY")
            
            if history:
                # Check for specific problem alarms
                alarm_codes_found = [e["code"] for e in history]
                
                if 0x33 in alarm_codes_found:
                    print("  🔴 ABSOLUTE POSITION ERROR (0x33) detected!")
                    print("     → ABZO home position information was damaged")
                    print("     → This causes position drift!")
                    print("     → Solution: Execute P-PRESET or return-to-home")
                
                if 0x44 in alarm_codes_found:
                    print("  🔴 ENCODER EEPROM ERROR (0x44) detected!")
                    print("     → ABZO sensor data was corrupted")
                    print("     → This causes position drift!")
                    print("     → Solution: Execute ZSG-PRESET or clear tripmeter")
                
                if 0x4A in alarm_codes_found:
                    print("  🔴 RETURN-TO-HOME INCOMPLETE (0x4A) detected!")
                    print("     → Position coordinate was not set")
                    print("     → This causes position drift!")
                    print("     → Solution: Perform position preset or return-to-home")
                
                if 0x25 in alarm_codes_found:
                    print("  🟡 UNDERVOLTAGE (0x25) detected!")
                    print("     → Power was cut off momentarily")
                    print("     → This can corrupt ABZO memory during write operations")
                
                if 0x22 in alarm_codes_found:
                    print("  🟡 OVERVOLTAGE (0x22) detected!")
                    print("     → Check power supply stability")
                
                if 0x84 in alarm_codes_found or 0x85 in alarm_codes_found:
                    print("  🟡 RS-485 COMMUNICATION ERROR detected!")
                    print("     → Communication issues can cause position read errors")
            else:
                print("  ✅ No position-affecting alarms found in history")
                print("  → Issue may be intermittent communication corruption")
                print("  → Run the monitor mode to catch it in real-time")
        
        finally:
            self.disconnect()
    
    def monitor_position(self, duration_seconds=60, interval_ms=100):
        """Monitor position for jumps in real-time."""
        if not self.connect():
            return
        
        self.print_separator(f"MONITORING POSITION (for {duration_seconds}s)")
        print("Watching for position jumps > 5000 steps...")
        print("Press Ctrl+C to stop\n")
        
        try:
            start_time = time.time()
            last_position = None
            jump_count = 0
            read_count = 0
            
            while (time.time() - start_time) < duration_seconds:
                pos = self.read_position()
                read_count += 1
                
                if pos:
                    current_pos = pos["position_steps"]
                    gimbal_deg = self.steps_to_degrees(current_pos)
                    
                    if last_position is not None:
                        delta = current_pos - last_position
                        
                        if abs(delta) > 5000:
                            jump_count += 1
                            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                            print(f"\n{'!'*60}")
                            print(f"⚠️  POSITION JUMP DETECTED at {timestamp}")
                            print(f"    Previous: {last_position:,} steps ({self.steps_to_degrees(last_position):.2f}°)")
                            print(f"    Current:  {current_pos:,} steps ({gimbal_deg:.2f}°)")
                            print(f"    Delta:    {delta:,} steps ({self.steps_to_degrees(delta):.2f}°)")
                            print(f"    Raw: Low=0x{pos['low_word']:04X} High=0x{pos['high_word']:04X}")
                            print(f"{'!'*60}\n")
                            
                            # Also check alarm status when jump occurs
                            alarm = self.read_alarm_status()
                            if alarm and alarm["code"] != 0:
                                print(f"    ALARM DURING JUMP: {alarm['hex']} - {alarm['description']}")
                        else:
                            # Normal operation indicator
                            if read_count % 20 == 0:
                                elapsed = int(time.time() - start_time)
                                print(f"  [{elapsed:3d}s] Position: {current_pos:>10,} steps ({gimbal_deg:>8.2f}°) | Jumps: {jump_count}", end='\r')
                    
                    last_position = current_pos
                
                time.sleep(interval_ms / 1000.0)
            
            print(f"\n\nMonitoring complete:")
            print(f"  Total reads: {read_count}")
            print(f"  Jumps detected: {jump_count}")
            
        except KeyboardInterrupt:
            print("\n\nMonitoring stopped by user.")
        finally:
            self.disconnect()


def main():
    import sys
    
    diag = AZDKDDiagnostic(CONFIG)
    
    if len(sys.argv) > 1 and sys.argv[1] == "monitor":
        # Monitor mode: watch for position jumps
        duration = int(sys.argv[2]) if len(sys.argv) > 2 else 60
        diag.monitor_position(duration_seconds=duration)
    else:
        # Default: run full diagnostic
        diag.run_full_diagnostic()


if __name__ == "__main__":
    main()