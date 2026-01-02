#!/usr/bin/env python3
"""
Oriental Motor AZD-KD Simple Alarm Reader
Uses INPUT registers (function code 4) which is often correct for monitoring data.

Your 0xFFFF result suggests you're reading from the wrong register type.
"""

import minimalmodbus

CONFIG = {
    "port": "/dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BC046FABCD-if04",
    "baudrate": 230400,
    "slave_id": 2,
    "timeout": 0.1,
}

ALARM_CODES = {
    0x00: "No alarm",
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
    0x33: "Absolute position error",
    0x34: "Command pulse error",
    0x41: "EEPROM error",
    0x42: "Sensor error at power on",
    0x43: "Rotation error at power on",
    0x44: "Encoder EEPROM error",
    0x45: "Motor combination error",
    0x4A: "Return-to-home incomplete",
    0x51: "Regeneration resistor overheat",
    0x60: "±LS both sides active",
    0x66: "Hardware overtravel",
    0x67: "Software overtravel",
    0x68: "HWTO input detection",
    0x70: "Operation data error",
    0x84: "RS-485 communication error",
    0x85: "RS-485 communication timeout",
    0xF0: "CPU error",
}


def main():
    print("="*60)
    print("AZD-KD ALARM READER")
    print("="*60)
    
    # Connect
    inst = minimalmodbus.Instrument(CONFIG["port"], CONFIG["slave_id"])
    inst.serial.baudrate = CONFIG["baudrate"]
    inst.serial.parity = minimalmodbus.serial.PARITY_NONE
    inst.serial.stopbits = 1
    inst.serial.bytesize = 8
    inst.serial.timeout = CONFIG["timeout"]
    inst.mode = minimalmodbus.MODE_RTU
    inst.clear_buffers_before_each_transaction = True
    
    print(f"Connected to {CONFIG['port']} @ {CONFIG['baudrate']} baud")
    print()
    
    # =========================================================================
    # TRY BOTH FUNCTION CODES ON KEY ADDRESSES
    # =========================================================================
    
    test_addresses = [
        # (address, name, expected_content)
        (0x0080, "Present Alarm", "alarm code"),
        (0x0081, "Alarm Detail", "sub-code"),
        (0x007C, "Alarm Status Alt", "alarm"),
        (0x007D, "Alarm Code Alt", "code"),
        (0x0040, "I/O Input", "digital inputs"),
        (0x0041, "I/O Output", "digital outputs"),
        (0x0044, "Present Alarm 2", "alarm"),
        (0x0046, "Warning Present", "warning"),
    ]
    
    print("Testing with HOLDING registers (FC3) vs INPUT registers (FC4):")
    print("-"*60)
    
    for addr, name, desc in test_addresses:
        print(f"\n{name} @ 0x{addr:04X} ({desc}):")
        
        # Try Function Code 3 (Holding Registers)
        try:
            val_fc3 = inst.read_registers(addr, 1, functioncode=3)
            fc3_str = f"0x{val_fc3[0]:04X}"
            if val_fc3[0] == 0xFFFF:
                fc3_str += " (invalid/no register)"
        except Exception as e:
            fc3_str = f"Error: {e}"
        
        # Try Function Code 4 (Input Registers)
        try:
            val_fc4 = inst.read_registers(addr, 1, functioncode=4)
            fc4_str = f"0x{val_fc4[0]:04X}"
            if val_fc4[0] == 0xFFFF:
                fc4_str += " (invalid/no register)"
            elif val_fc4[0] in ALARM_CODES:
                fc4_str += f" → {ALARM_CODES[val_fc4[0]]}"
            elif val_fc4[0] != 0 and val_fc4[0] < 0x100:
                fc4_str += f" → Possible alarm 0x{val_fc4[0]:02X}"
        except Exception as e:
            fc4_str = f"Error: {e}"
        
        print(f"  FC3 (Holding): {fc3_str}")
        print(f"  FC4 (Input):   {fc4_str}")
    
    # =========================================================================
    # READ ALARM HISTORY (try both FCs at common addresses)
    # =========================================================================
    print("\n" + "="*60)
    print("ALARM HISTORY")
    print("="*60)
    
    history_addrs = [0x0096, 0x0102, 0x0086]
    
    for base_addr in history_addrs:
        print(f"\nTrying history at 0x{base_addr:04X}:")
        
        for fc in [3, 4]:
            try:
                # Read 10 entries (2 registers each)
                regs = inst.read_registers(base_addr, 20, functioncode=fc)
                
                valid_entries = []
                for i in range(10):
                    code = regs[i*2 + 1]  # Low word typically has alarm code
                    if code != 0 and code != 0xFFFF:
                        desc = ALARM_CODES.get(code, f"Unknown 0x{code:02X}")
                        valid_entries.append(f"  [{i+1}] 0x{code:02X}: {desc}")
                
                if valid_entries:
                    print(f"  FC{fc}: Found {len(valid_entries)} alarm(s):")
                    for entry in valid_entries:
                        print(entry)
                else:
                    print(f"  FC{fc}: No alarms or invalid")
                    
            except Exception as e:
                print(f"  FC{fc}: Error - {e}")
    
    # =========================================================================
    # QUICK POSITION/STATUS CHECK
    # =========================================================================
    print("\n" + "="*60)
    print("QUICK STATUS CHECK")
    print("="*60)
    
    # Position at 0x0000
    try:
        pos_regs = inst.read_registers(0x0000, 2, functioncode=4)
        pos = (pos_regs[0] << 16) | pos_regs[1]
        if pos >= 0x80000000:
            pos -= 0x100000000
        print(f"Position (FC4): {pos} steps")
    except:
        pass
    
    try:
        pos_regs = inst.read_registers(0x0000, 2, functioncode=3)
        pos = (pos_regs[0] << 16) | pos_regs[1]
        if pos >= 0x80000000:
            pos -= 0x100000000
        print(f"Position (FC3): {pos} steps")
    except:
        pass
    
    # Close
    inst.serial.close()
    print("\nDone.")


if __name__ == "__main__":
    main()