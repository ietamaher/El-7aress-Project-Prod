#!/usr/bin/env python3
"""
Oriental Motor AZD-KD Servo Driver Diagnostic Tool
Reads alarm history, position, temperatures, and monitors for issues.

Based on HM-ALARM.pdf and HM-ALARM-2.pdf register maps.

Install: pip install minimalmodbus pyserial
"""

import time
from datetime import datetime
import minimalmodbus
import sys

# ============================================================================
# CONFIGURATION
# ============================================================================
CONFIG = {
    "port": "/dev/servo_el",
    "baudrate": 230400,
    "parity": "N",
    "stopbits": 1,
    "bytesize": 8,
    "slave_id": 1,
    "timeout": 0.5,
}

# ============================================================================
# AZD-KD REGISTER MAP (from HM-ALARM-2.pdf and your C++ code)
# ============================================================================
REGISTERS = {
    # Position and speed (Read) - Your existing addresses
    "POSITION": 204,                 # 4 registers for 2 axes
    "POSITION_REG_COUNT": 4,
    
    # Temperature (Read) - Address 248 (0x00F8)
    "DRIVER_TEMP": 248,              # 2 registers (high, low) - x0.1 °C
    "MOTOR_TEMP": 250,               # 2 registers (high, low) - x0.1 °C
    "TEMPERATURE_REG_COUNT": 4,
    
    # Alarm status (Read) - from HM-ALARM-2.pdf page 1
    "PRESENT_ALARM": 128,            # 0x0080 - 2 registers
    "ALARM_HISTORY_START": 130,      # 0x0082 - Alarm history 1
    "ALARM_HISTORY_REG_COUNT": 20,   # 10 entries × 2 registers each
    
    # Communication error history
    "COMM_ERROR_PRESENT": 172,       # 0x00AC
    "COMM_ERROR_HISTORY_START": 174, # 0x00AE
    
    # Maintenance commands (Write)
    "ALARM_RESET": 384,              # 0x0180 - Write 1 to reset
    "ALARM_HISTORY_CLEAR": 388,      # 0x0184 - Write 1 to clear
    "P_PRESET": 394,                 # 0x018A - Position preset
}

# ============================================================================
# COMPLETE ALARM CODE DEFINITIONS (from HM-ALARM.pdf)
# ============================================================================
ALARM_CODES = {
    0x10: {
        "name": "Excessive position deviation",
        "cause": "Deviation between command and feedback position exceeded limit",
        "action": "Decrease load, increase accel/decel time, increase operating current",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 4
    },
    0x20: {
        "name": "Overcurrent",
        "cause": "Motor, cable, or driver output circuit short-circuited",
        "action": "Turn off power, check motor/cable/driver, cycle power",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 5
    },
    0x21: {
        "name": "Main circuit overheat",
        "cause": "Internal temperature reached upper limit (85°C)",
        "action": "Review ventilation condition",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 2
    },
    0x22: {
        "name": "Overvoltage",
        "cause": "Power supply voltage exceeded allowable value (AC:430V, DC:63V)",
        "action": "Check input voltage, decrease load, connect regeneration resistor",
        "can_reset": False,  # AC driver: False, DC driver: True
        "affects_position": False,
        "led_blinks": 3
    },
    0x23: {
        "name": "Main power supply OFF",
        "cause": "Main power supply shut off while operating",
        "action": "Check if main power supply is applied normally",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 3
    },
    0x25: {
        "name": "Undervoltage",
        "cause": "Power was cut off momentarily or voltage became low",
        "action": "Check input voltage of power supply",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 3
    },
    0x26: {
        "name": "Motor overheat",
        "cause": "ABZO sensor temperature reached upper limit (85°C)",
        "action": "Check motor heat radiation, review ventilation",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 8
    },
    0x28: {
        "name": "Sensor error",
        "cause": "Sensor error detected during operation",
        "action": "Turn off power, check motor connection, cycle power",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 8
    },
    0x2A: {
        "name": "ABZO sensor communication error",
        "cause": "Communication error between driver and ABZO sensor",
        "action": "Turn off power, check ABZO sensor connection, cycle power",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 8
    },
    0x30: {
        "name": "Overload",
        "cause": "Load exceeding max torque applied for too long",
        "action": "Decrease load, increase accel/decel time, increase operating current",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 2
    },
    0x31: {
        "name": "Overspeed",
        "cause": "Feedback speed exceeded specified value",
        "action": "Review electronic gear parameter, slow acceleration",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 2
    },
    0x33: {
        "name": "⚠️  ABSOLUTE POSITION ERROR",
        "cause": "Home position information of ABZO sensor was DAMAGED",
        "action": "Perform position preset or return-to-home operation",
        "can_reset": False,
        "affects_position": True,
        "led_blinks": 7
    },
    0x34: {
        "name": "Command pulse error",
        "cause": "Command pulse frequency exceeded 38,400 r/min",
        "action": "Decrease command pulse frequency",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 2
    },
    0x41: {
        "name": "EEPROM error",
        "cause": "Data stored in driver was damaged",
        "action": "Initialize all parameters",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 9
    },
    0x42: {
        "name": "Sensor error at power on",
        "cause": "ABZO sensor error detected at power on",
        "action": "Turn off power, check ABZO sensor connection, cycle power",
        "can_reset": False,
        "affects_position": True,
        "led_blinks": 8
    },
    0x43: {
        "name": "Rotation error at power on",
        "cause": "Motor was rotating when power was turned on",
        "action": "Ensure motor shaft doesn't rotate at power on",
        "can_reset": False,
        "affects_position": True,
        "led_blinks": 8
    },
    0x44: {
        "name": "⚠️  ENCODER EEPROM ERROR",
        "cause": "Data stored in ABZO sensor was CORRUPTED",
        "action": "Execute ZSG-PRESET or clear tripmeter. If persists, ABZO damaged",
        "can_reset": False,
        "affects_position": True,
        "led_blinks": 8
    },
    0x45: {
        "name": "Motor combination error",
        "cause": "Motor not supported by driver is connected",
        "action": "Check motor and driver model, connect correct combination",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 8
    },
    0x4A: {
        "name": "⚠️  RETURN-TO-HOME INCOMPLETE",
        "cause": "Absolute positioning started when position coordinate NOT SET",
        "action": "Perform position preset or return-to-home operation",
        "can_reset": True,
        "affects_position": True,
        "led_blinks": 7
    },
    0x51: {
        "name": "Regeneration resistor overheat",
        "cause": "Regen resistor not connected correctly or overheated",
        "action": "Short TH1/TH2 if no resistor, or connect correctly",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 2
    },
    0x53: {
        "name": "HWTO input circuit error",
        "cause": "HWTO1/HWTO2 timing exceeded delay parameter",
        "action": "Increase HWTO delay time, check HWTO wiring",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 2
    },
    0x60: {
        "name": "±LS both sides active",
        "cause": "Both FW-LS and RV-LS inputs detected",
        "action": "Check sensor logic and inverting mode parameter",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x61: {
        "name": "Reverse ±LS connection",
        "cause": "Opposite LS input detected during return-to-home",
        "action": "Check sensor wiring",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x62: {
        "name": "Return-to-home operation error",
        "cause": "Load or sensor position issue during return-to-home",
        "action": "Check load, review sensor positions",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x63: {
        "name": "No HOMES",
        "cause": "HOMES input not detected between FW-LS and RV-LS",
        "action": "Install HOME sensor between limit sensors",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x64: {
        "name": "TIM, Z, SLIT signal error",
        "cause": "None of TIM/ZSG/SLIT signals detected during homing",
        "action": "Check signals or disable unused ones",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x66: {
        "name": "Hardware overtravel",
        "cause": "FW-LS or RV-LS input detected with alarm enabled",
        "action": "After reset, escape from sensor",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x67: {
        "name": "Software overtravel",
        "cause": "Motor reached software limit with alarm enabled",
        "action": "Review operation data, escape after reset",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x68: {
        "name": "HWTO input detection",
        "cause": "HWTO1 or HWTO2 input turned OFF",
        "action": "Turn HWTO1 and HWTO2 inputs ON",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 1
    },
    0x6A: {
        "name": "Return-to-home offset error",
        "cause": "LS detected during offset movement in homing",
        "action": "Check offset value",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x6D: {
        "name": "Mechanical overtravel",
        "cause": "Reached mechanism limit stored in ABZO sensor",
        "action": "Check travel amount, escape after reset",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x70: {
        "name": "Operation data error",
        "cause": "Speed 0, wrap disabled, or mechanism protection exceeded",
        "action": "Check operation data, wrap setting",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x71: {
        "name": "Electronic gear setting error",
        "cause": "Resolution out of specification",
        "action": "Review electronic gear parameter",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 7
    },
    0x72: {
        "name": "Wrap setting error",
        "cause": "Wrap setting inconsistent with electronic gear",
        "action": "Set wrap correctly and cycle power",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 7
    },
    0x81: {
        "name": "Network bus error",
        "cause": "Master controller disconnected during operation",
        "action": "Check master controller connector/cable",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x83: {
        "name": "Communication switch setting error",
        "cause": "BAUD switch out of specification",
        "action": "Check BAUD switch",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 7
    },
    0x84: {
        "name": "RS-485 communication error",
        "cause": "Consecutive RS-485 errors reached limit",
        "action": "Check master-driver connection, RS-485 settings",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x85: {
        "name": "RS-485 communication timeout",
        "cause": "Communication timeout elapsed",
        "action": "Check master-driver connection",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0x8E: {
        "name": "Network converter error",
        "cause": "Alarm generated in network converter",
        "action": "Check network converter alarm code",
        "can_reset": True,
        "affects_position": False,
        "led_blinks": 7
    },
    0xF0: {
        "name": "CPU error",
        "cause": "CPU malfunctioned",
        "action": "Cycle the power",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 0  # LED stays lit
    },
}

# Critical alarms that affect position
POSITION_AFFECTING_ALARMS = [0x33, 0x44, 0x4A, 0x42, 0x43]


def get_alarm_info(code):
    """Get alarm information for a given code."""
    if code in ALARM_CODES:
        return ALARM_CODES[code]
    return {
        "name": f"Unknown alarm (0x{code:02X})",
        "cause": "Unknown alarm code",
        "action": "Check Oriental Motor documentation",
        "can_reset": False,
        "affects_position": False,
        "led_blinks": 0
    }


class AZDKDDiagnostic:
    def __init__(self, config):
        self.config = config
        self.instrument = None
        
    def connect(self):
        """Establish Modbus RTU connection."""
        print(f"\n{'='*70}")
        print("CONNECTING TO AZD-KD SERVO DRIVER")
        print(f"{'='*70}")
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
            print(f"  ❌ Read error at 0x{address:04X}: {e}")
            return None
    
    def read_32bit_signed(self, address):
        """Read 32-bit signed integer from 2 registers (Big Endian)."""
        regs = self.read_registers(address, 2)
        if regs is None:
            return None
        
        # Oriental Motor: Register 0 = High word, Register 1 = Low word
        high_word = regs[0]
        low_word = regs[1]
        
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
    
    def read_temperatures(self):
        """Read driver and motor temperatures."""
        print("\n  Reading temperature registers...")
        
        # Read all 4 temperature registers at once
        regs = self.read_registers(REGISTERS["DRIVER_TEMP"], 4)
        
        result = {
            "driver_temp": None,
            "motor_temp": None,
            "driver_raw": None,
            "motor_raw": None,
            "all_raw": regs
        }
        
        if regs and len(regs) >= 4:
            result["driver_raw"] = regs[0:2]
            result["motor_raw"] = regs[2:4]
            
            # Temperature in low word, scaled by 0.1
            result["driver_temp"] = regs[1] * 0.1
            result["motor_temp"] = regs[3] * 0.1
            
            print(f"    Raw registers: {[f'0x{r:04X}' for r in regs]}")
        
        return result
    
    def read_alarm_status(self):
        """Read current alarm status."""
        regs = self.read_registers(REGISTERS["PRESENT_ALARM"], 2)
        if regs is None:
            return None
        
        # Alarm code typically in low word
        alarm_code = regs[1] & 0xFF  # Lower byte usually contains the code
        
        info = get_alarm_info(alarm_code)
        
        return {
            "code": alarm_code,
            "hex": f"0x{alarm_code:02X}",
            "name": info["name"],
            "cause": info["cause"],
            "action": info["action"],
            "can_reset": info["can_reset"],
            "affects_position": info["affects_position"],
            "led_blinks": info["led_blinks"],
            "raw_high": regs[0],
            "raw_low": regs[1]
        }
    
    def read_alarm_history(self):
        """Read alarm history (up to 10 entries)."""
        history = []
        
        # Read all history registers at once
        regs = self.read_registers(
            REGISTERS["ALARM_HISTORY_START"], 
            REGISTERS["ALARM_HISTORY_REG_COUNT"]
        )
        
        if regs is None:
            return history
        
        # Parse 10 alarm entries (2 registers each)
        for i in range(10):
            idx = i * 2
            if idx + 1 >= len(regs):
                break
            
            high_word = regs[idx]
            low_word = regs[idx + 1]
            alarm_code = low_word & 0xFF
            
            if alarm_code != 0 and alarm_code != 0xFF:
                info = get_alarm_info(alarm_code)
                history.append({
                    "index": i + 1,
                    "code": alarm_code,
                    "hex": f"0x{alarm_code:02X}",
                    "name": info["name"],
                    "cause": info["cause"],
                    "action": info["action"],
                    "can_reset": info["can_reset"],
                    "affects_position": info["affects_position"],
                    "led_blinks": info["led_blinks"],
                    "raw_high": high_word,
                    "raw_low": low_word
                })
        
        return history
    
    def read_comm_error_history(self):
        """Read communication error history."""
        errors = []
        
        # Read current comm error
        current = self.read_registers(REGISTERS["COMM_ERROR_PRESENT"], 2)
        if current:
            if current[1] != 0:
                errors.append({
                    "index": 0,
                    "type": "Current",
                    "code": current[1],
                    "raw": current
                })
        
        # Read history (9 entries)
        history_regs = self.read_registers(REGISTERS["COMM_ERROR_HISTORY_START"], 18)
        if history_regs:
            for i in range(9):
                idx = i * 2
                if idx + 1 >= len(history_regs):
                    break
                code = history_regs[idx + 1]
                if code != 0 and code != 0xFFFF:
                    errors.append({
                        "index": i + 1,
                        "type": "History",
                        "code": code,
                        "raw": [history_regs[idx], history_regs[idx + 1]]
                    })
        
        return errors
    
    def steps_to_degrees(self, steps):
        """Convert motor steps to gimbal degrees."""
        gear_ratio = 174.0 / 34.0  # 5.117
        motor_step_deg = 0.009     # degrees per step
        deg_per_step = motor_step_deg / gear_ratio
        return steps * deg_per_step
    
    def print_separator(self, title):
        """Print section separator."""
        print(f"\n{'='*70}")
        print(f" {title}")
        print(f"{'='*70}")
    
    def print_alarm_details(self, alarm, indent="  "):
        """Print detailed alarm information."""
        print(f"{indent}Code:    {alarm['hex']} ({alarm['code']})")
        print(f"{indent}Name:    {alarm['name']}")
        print(f"{indent}Cause:   {alarm['cause']}")
        print(f"{indent}Action:  {alarm['action']}")
        print(f"{indent}LED:     Blinks {alarm['led_blinks']} times" if alarm['led_blinks'] > 0 else f"{indent}LED:     Stays lit")
        print(f"{indent}Reset:   {'ALM-RST possible' if alarm['can_reset'] else 'Power cycle required'}")
        if alarm['affects_position']:
            print(f"{indent}⚠️  THIS ALARM AFFECTS POSITION DATA!")
    
    def run_full_diagnostic(self):
        """Run complete diagnostic."""
        if not self.connect():
            return
        
        try:
            # ============================================================
            # 1. TEMPERATURES
            # ============================================================
            self.print_separator("TEMPERATURES")
            temps = self.read_temperatures()
            
            if temps["driver_temp"] is not None:
                status = "🔴 HOT!" if temps["driver_temp"] > 70 else "✅"
                print(f"  Driver: {temps['driver_temp']:.1f} °C {status}")
                print(f"    Raw: High=0x{temps['driver_raw'][0]:04X} Low=0x{temps['driver_raw'][1]:04X}")
            else:
                print("  Driver: ❌ Read failed")
            
            if temps["motor_temp"] is not None:
                status = "🔴 HOT!" if temps["motor_temp"] > 70 else "✅"
                print(f"  Motor:  {temps['motor_temp']:.1f} °C {status}")
                print(f"    Raw: High=0x{temps['motor_raw'][0]:04X} Low=0x{temps['motor_raw'][1]:04X}")
            else:
                print("  Motor: ❌ Read failed")
            
            # ============================================================
            # 2. CURRENT POSITION
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
                
                if abs(pos["position_steps"]) > 10000000:
                    print("  ⚠️  WARNING: Position value seems abnormally large!")
            
            # ============================================================
            # 3. CURRENT ALARM STATUS
            # ============================================================
            self.print_separator("CURRENT ALARM STATUS")
            alarm = self.read_alarm_status()
            if alarm:
                print(f"  Raw registers: High=0x{alarm['raw_high']:04X} Low=0x{alarm['raw_low']:04X}")
                if alarm["code"] == 0:
                    print("  ✅ No active alarm")
                else:
                    print(f"  ❌ ACTIVE ALARM!")
                    self.print_alarm_details(alarm, "     ")
            
            # ============================================================
            # 4. ALARM HISTORY (CRITICAL!)
            # ============================================================
            self.print_separator("ALARM HISTORY (Last 10 alarms)")
            history = self.read_alarm_history()
            
            if not history:
                print("  ✅ No alarms in history")
            else:
                position_alarms_found = False
                
                for entry in history:
                    status = "⚠️ POSITION!" if entry["affects_position"] else ""
                    reset_info = "✓RST" if entry["can_reset"] else "PWR"
                    print(f"\n  [{entry['index']:2d}] {entry['hex']}: {entry['name']} [{reset_info}] {status}")
                    print(f"       Cause: {entry['cause']}")
                    
                    if entry["affects_position"]:
                        position_alarms_found = True
                
                if position_alarms_found:
                    print("\n  " + "!"*60)
                    print("  !!! POSITION-AFFECTING ALARMS FOUND IN HISTORY !!!")
                    print("  !!! This may cause position drift !!!")
                    print("  " + "!"*60)
            
            # ============================================================
            # 5. COMMUNICATION ERROR HISTORY
            # ============================================================
            self.print_separator("COMMUNICATION ERROR HISTORY")
            comm_errors = self.read_comm_error_history()
            
            if not comm_errors:
                print("  ✅ No communication errors in history")
            else:
                for err in comm_errors:
                    print(f"  [{err['index']:2d}] {err['type']}: Code 0x{err['code']:04X}")
                    print(f"       Raw: {[f'0x{r:04X}' for r in err['raw']]}")
            
            # ============================================================
            # 6. ALL AVAILABLE ALARM CODES (Reference)
            # ============================================================
            self.print_separator("ALARM CODE REFERENCE")
            print("  Position-affecting alarms (cause drift):")
            for code in POSITION_AFFECTING_ALARMS:
                info = get_alarm_info(code)
                print(f"    0x{code:02X}: {info['name']}")
            
            print("\n  Other critical alarms:")
            for code in [0x20, 0x21, 0x22, 0x25, 0x26, 0x28, 0x2A, 0x84, 0x85]:
                info = get_alarm_info(code)
                print(f"    0x{code:02X}: {info['name']}")
            
            # ============================================================
            # 7. DIAGNOSIS SUMMARY
            # ============================================================
            self.print_separator("DIAGNOSIS SUMMARY")
            
            issues_found = []
            
            # Check temperatures
            if temps["driver_temp"] and temps["driver_temp"] > 70:
                issues_found.append(f"🔴 Driver temperature high: {temps['driver_temp']:.1f}°C")
            if temps["motor_temp"] and temps["motor_temp"] > 70:
                issues_found.append(f"🔴 Motor temperature high: {temps['motor_temp']:.1f}°C")
            
            # Check current alarm
            if alarm and alarm["code"] != 0:
                issues_found.append(f"🔴 Active alarm: {alarm['hex']} - {alarm['name']}")
            
            # Check alarm history for critical issues
            if history:
                alarm_codes_found = [e["code"] for e in history]
                
                if 0x33 in alarm_codes_found:
                    issues_found.append("🔴 ABSOLUTE POSITION ERROR (0x33) - ABZO home damaged")
                if 0x44 in alarm_codes_found:
                    issues_found.append("🔴 ENCODER EEPROM ERROR (0x44) - ABZO data corrupted")
                if 0x4A in alarm_codes_found:
                    issues_found.append("🔴 RETURN-TO-HOME INCOMPLETE (0x4A) - Position not set")
                if 0x25 in alarm_codes_found:
                    issues_found.append("🟡 UNDERVOLTAGE (0x25) - Power instability")
                if 0x22 in alarm_codes_found:
                    issues_found.append("🟡 OVERVOLTAGE (0x22) - Check power supply")
                if 0x84 in alarm_codes_found or 0x85 in alarm_codes_found:
                    issues_found.append("🟡 RS-485 COMMUNICATION ERROR - Check wiring")
            
            if issues_found:
                print("  Issues found:")
                for issue in issues_found:
                    print(f"    • {issue}")
                
                print("\n  Recommended actions:")
                if any("POSITION" in i or "ABZO" in i or "ENCODER" in i for i in issues_found):
                    print("    1. Execute P-PRESET or return-to-home operation")
                    print("    2. If 0x44 persists: ZSG-PRESET or clear tripmeter")
                    print("    3. Check for power stability issues")
                if any("temperature" in i.lower() for i in issues_found):
                    print("    → Check ventilation and heat dissipation")
                if any("COMMUNICATION" in i for i in issues_found):
                    print("    → Check RS-485 wiring and termination")
            else:
                print("  ✅ No critical issues detected")
                print("  → System appears to be operating normally")
        
        finally:
            self.disconnect()
    
    def monitor_realtime(self, duration_seconds=60, interval_ms=500):
        """Monitor temperatures and alarms in real-time."""
        if not self.connect():
            return
        
        self.print_separator(f"REAL-TIME MONITORING ({duration_seconds}s)")
        print("Press Ctrl+C to stop\n")
        print(f"{'Time':<12} {'Driver°C':<10} {'Motor°C':<10} {'Position':<15} {'Alarm':<10}")
        print("-" * 60)
        
        try:
            start_time = time.time()
            last_position = None
            
            while (time.time() - start_time) < duration_seconds:
                timestamp = datetime.now().strftime("%H:%M:%S")
                
                # Read temperatures
                temps = self.read_registers(REGISTERS["DRIVER_TEMP"], 4)
                driver_temp = temps[1] * 0.1 if temps else 0
                motor_temp = temps[3] * 0.1 if temps and len(temps) > 3 else 0
                
                # Read position
                pos = self.read_position()
                pos_str = f"{pos['position_steps']:,}" if pos else "ERROR"
                
                # Read alarm
                alarm = self.read_alarm_status()
                alarm_str = alarm['hex'] if alarm and alarm['code'] != 0 else "OK"
                
                # Check for position jump
                jump_marker = ""
                if pos and last_position is not None:
                    delta = abs(pos['position_steps'] - last_position)
                    if delta > 5000:
                        jump_marker = f" ⚠️ JUMP: {delta:,}"
                
                if pos:
                    last_position = pos['position_steps']
                
                print(f"{timestamp:<12} {driver_temp:<10.1f} {motor_temp:<10.1f} {pos_str:<15} {alarm_str:<10}{jump_marker}")
                
                time.sleep(interval_ms / 1000.0)
        
        except KeyboardInterrupt:
            print("\n\nMonitoring stopped.")
        finally:
            self.disconnect()


def print_usage():
    """Print usage information."""
    print("""
Oriental Motor AZD-KD Diagnostic Tool
=====================================

Usage:
  python azd_kd_diagnostic.py [command] [options]

Commands:
  (none)      Run full diagnostic (default)
  monitor     Real-time monitoring mode
  alarms      List all alarm codes
  help        Show this help

Examples:
  python azd_kd_diagnostic.py
  python azd_kd_diagnostic.py monitor 120
  python azd_kd_diagnostic.py alarms

Configuration:
  Edit CONFIG dict in script to change port, baudrate, slave_id
""")


def list_all_alarms():
    """Print all alarm codes and descriptions."""
    print("\n" + "="*80)
    print(" ORIENTAL MOTOR AZD-KD ALARM CODE REFERENCE")
    print("="*80)
    
    print("\n⚠️  POSITION-AFFECTING ALARMS (cause position drift):")
    print("-"*80)
    for code in sorted(POSITION_AFFECTING_ALARMS):
        info = ALARM_CODES.get(code, {})
        print(f"\n  0x{code:02X} - {info.get('name', 'Unknown')}")
        print(f"    Cause:  {info.get('cause', 'Unknown')}")
        print(f"    Action: {info.get('action', 'Unknown')}")
        print(f"    Reset:  {'ALM-RST' if info.get('can_reset') else 'Power cycle'}")
    
    print("\n\n📋 ALL OTHER ALARMS:")
    print("-"*80)
    for code in sorted(ALARM_CODES.keys()):
        if code not in POSITION_AFFECTING_ALARMS:
            info = ALARM_CODES[code]
            reset = "✓" if info['can_reset'] else "✗"
            print(f"  0x{code:02X} [{reset}] {info['name']}")


def main():
    if len(sys.argv) > 1:
        cmd = sys.argv[1].lower()
        
        if cmd == "help" or cmd == "-h" or cmd == "--help":
            print_usage()
            return
        
        if cmd == "alarms":
            list_all_alarms()
            return
        
        if cmd == "monitor":
            duration = int(sys.argv[2]) if len(sys.argv) > 2 else 60
            diag = AZDKDDiagnostic(CONFIG)
            diag.monitor_realtime(duration_seconds=duration)
            return
    
    # Default: full diagnostic
    diag = AZDKDDiagnostic(CONFIG)
    diag.run_full_diagnostic()


if __name__ == "__main__":
    main()