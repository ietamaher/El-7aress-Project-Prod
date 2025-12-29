#!/usr/bin/env python3
"""
Day Camera Zoom Command Tester
Tests zoom commands, status queries, and position setting
"""

import serial
import time
import json
import os

def load_config():
    config_path = os.path.join(os.path.dirname(__file__), '..', 'config', 'devices.json')
    with open(config_path, 'r') as f:
        config = json.load(f)
        return config['video']['dayCamera']

def build_pelco_d_command(address, cmd1, cmd2, data1=0x00, data2=0x00):
    """Build Pelco-D command exactly like C++"""
    packet = bytearray([0xFF, address, cmd1, cmd2, data1, data2])
    checksum = (address + cmd1 + cmd2 + data1 + data2) & 0xFF
    packet.append(checksum)
    return bytes(packet)

def parse_zoom_response(response):
    """Parse zoom position from response"""
    if len(response) >= 7 and response[0] == 0xFF:
        # Validate checksum
        addr = response[1]
        resp1 = response[2]
        resp2 = response[3]
        data1 = response[4]
        data2 = response[5]
        recv_checksum = response[6]
        calc_checksum = (addr + resp1 + resp2 + data1 + data2) & 0xFF
        
        if recv_checksum == calc_checksum:
            if resp2 == 0xA7:  # Zoom position response
                zoom_pos = (data1 << 8) | data2
                
                # Calculate HFOV like C++ code
                max_zoom = 0x4000  # 16384
                fraction = min(zoom_pos / max_zoom, 1.0)
                wide_hfov = 63.7
                tele_hfov = 2.3
                hfov = wide_hfov - (wide_hfov - tele_hfov) * fraction
                
                return zoom_pos, hfov
    return None, None

def test_get_camera_status(ser, camera_address):
    """Test getCameraStatus() - Using Focus Far command to query zoom"""
    print("\n" + "=" * 80)
    print("TEST: getCameraStatus() - Query zoom via Focus Far command")
    print("=" * 80)
    
    # NEW METHOD: Use Focus Far command (0x00, 0x02) to query zoom position
    cmd = build_pelco_d_command(camera_address, 0x00, 0x02, 0x00, 0x00)
    print(f"Command: FF {camera_address:02X} 00 02 00 00 [checksum] (Focus Far)")
    print(f"→ TX: {' '.join(f'{b:02X}' for b in cmd)}")
    
    ser.reset_input_buffer()
    ser.write(cmd)
    ser.flush()
    time.sleep(0.05)  # Short delay, then read
    
    resp = ser.read(128)
    if resp:
        print(f"← RX: {' '.join(f'{b:02X}' for b in resp)} ({len(resp)} bytes)")
        
        # Check if response contains zoom position (resp[3] == 0xA7)
        if len(resp) >= 7 and resp[3] == 0xA7:
            zoom_pos, hfov = parse_zoom_response(resp)
            if zoom_pos is not None:
                print(f"✓ Zoom Position: {zoom_pos} (0x{zoom_pos:04X})")
                print(f"✓ HFOV: {hfov:.2f}°")
                return zoom_pos
            else:
                print("✗ Response has 0xA7 but failed checksum or parsing")
                return None
        else:
            print(f"✗ Response byte[3] = 0x{resp[3]:02X} (expected 0xA7 for zoom data)")
            return None
    else:
        print("✗ No response received")
        return None

def test_set_zoom_position(ser, camera_address, target_position):
    """Test setZoomPosition() - Command camera to specific zoom"""
    print("\n" + "=" * 80)
    print(f"TEST: setZoomPosition({target_position}) - Command zoom to position")
    print("=" * 80)
    
    # This is what setZoomPosition() does in C++
    high = (target_position >> 8) & 0xFF
    low = target_position & 0xFF
    cmd = build_pelco_d_command(camera_address, 0x00, 0xA7, high, low)
    print(f"Command: FF {camera_address:02X} 00 A7 {high:02X} {low:02X} [checksum]")
    print(f"→ TX: {' '.join(f'{b:02X}' for b in cmd)}")
    
    ser.reset_input_buffer()
    ser.write(cmd)
    ser.flush()
    time.sleep(0.3)  # Wait for response
    
    resp = ser.read(64)
    if resp:
        print(f"← RX: {' '.join(f'{b:02X}' for b in resp)}")
        zoom_pos, hfov = parse_zoom_response(resp)
        if zoom_pos is not None:
            print(f"✓ Camera responded with position: {zoom_pos} (0x{zoom_pos:04X})")
            print(f"✓ HFOV: {hfov:.2f}°")
            if zoom_pos == target_position:
                print(f"✓ Position MATCHES target!")
            else:
                print(f"⚠ Position differs from target (camera may be moving)")
            return zoom_pos
        else:
            print("✗ Invalid or no zoom position in response")
            return None
    else:
        print("✗ No response received")
        return None

def main():
    config = load_config()
    SERIAL_PORT = config['controlPort']
    BAUD_RATE = 9600
    CAMERA_ADDRESS = 0x01
    
    print("=" * 80)
    print("  DAY CAMERA PROTOCOL TESTER")
    print("  Tests: Zoom Movement, Status Query, Position Setting")
    print("=" * 80)
    print(f"Port:    {SERIAL_PORT}")
    print(f"Baud:    {BAUD_RATE}")
    print(f"Address: 0x{CAMERA_ADDRESS:02X}")
    print("=" * 80)
    print("\n⚠️  IMPORTANT: Make sure Qt app is NOT running!")
    print("    (Port can only be used by one application)\n")
    input("Press ENTER when ready...")
    
    try:
        print("\nOpening serial port...")
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1.0) as ser:
            print("✓ Port opened successfully\n")
            time.sleep(0.5)
            
            # ================================================================
            # PART 1: Test getCameraStatus() - GET current position
            # ================================================================
            initial_pos = test_get_camera_status(ser, CAMERA_ADDRESS)
            time.sleep(1)
            
            # ================================================================
            # PART 2: Test setZoomPosition() - SET a specific position
            # ================================================================
            # Try to set zoom to mid-range (8192 = 0x2000 = 50% zoom)
            target_pos = 8192
            result_pos = test_set_zoom_position(ser, CAMERA_ADDRESS, target_pos)
            
            # Wait for camera to move
            print("\n⏳ Waiting 3 seconds for camera to reach position...")
            time.sleep(3)
            
            # Query again to see if it reached the target
            final_pos = test_get_camera_status(ser, CAMERA_ADDRESS)
            
            # ================================================================
            # PART 3: Manual zoom and observe STOP response
            # ================================================================
            print("\n" + "=" * 80)
            print("MANUAL ZOOM TEST - Observe STOP command response")
            print("=" * 80)
            
            # Zoom Out
            cmd = build_pelco_d_command(CAMERA_ADDRESS, 0x00, 0x40)
            print("\n[1] ZOOM OUT (cmd1=0x00, cmd2=0x40)")
            print(f"    → TX: {' '.join(f'{b:02X}' for b in cmd)}")
            ser.reset_input_buffer()
            ser.write(cmd)
            ser.flush()
            time.sleep(0.1)
            resp = ser.read(64)
            if resp:
                print(f"    ← RX: {' '.join(f'{b:02X}' for b in resp)}")
            else:
                print(f"    ← No response (normal for zoom start)")
            
            print("    ⏳ Zooming out for 2 seconds...")
            time.sleep(2)
            
            # Zoom Stop
            cmd = build_pelco_d_command(CAMERA_ADDRESS, 0x00, 0x00)
            print("\n[2] ZOOM STOP (cmd1=0x00, cmd2=0x00)")
            print(f"    → TX: {' '.join(f'{b:02X}' for b in cmd)}")
            ser.reset_input_buffer()
            ser.write(cmd)
            ser.flush()
            time.sleep(0.3)
            resp = ser.read(64)
            if resp:
                print(f"    ← RX: {' '.join(f'{b:02X}' for b in resp)}")
                zoom_pos, hfov = parse_zoom_response(resp)
                if zoom_pos is not None:
                    print(f"    ✓ Zoom Position: {zoom_pos} (0x{zoom_pos:04X})")
                    print(f"    ✓ HFOV: {hfov:.2f}°")
            else:
                print(f"    ← No response")
            
            # ================================================================
            # SUMMARY
            # ================================================================
            print("\n" + "=" * 80)
            print("TEST SUMMARY")
            print("=" * 80)
            print("\nCOMMAND COMPARISON:")
            print(f"  getCameraStatus():        FF {CAMERA_ADDRESS:02X} 00 02 00 00 [cs] (Focus Far)")
            print(f"  setZoomPosition(8192):    FF {CAMERA_ADDRESS:02X} 00 A7 20 00 [cs]")
            print(f"  zoomStop():               FF {CAMERA_ADDRESS:02X} 00 00 00 00 [cs]")
            
            print("\nKEY FINDINGS:")
            print("  1. getCameraStatus() using Focus Far (0x00, 0x02) - NEW METHOD")
            print("     → Camera returns zoom position in response!")
            
            print("\n  2. setZoomPosition(pos) (0x00, 0xA7, high, low) = SET command")
            print("     → Commands 'move to this zoom position'")
            
            print("\n  3. zoomStop() (0x00, 0x00, 0x00, 0x00) = STOP command")
            print("     → Also returns current position as side effect")
            
            print("\n  4. Focus commands seem to trigger zoom position response:")
            print("     → Focus Far (0x00, 0x02) returns current zoom")
            print("     → This is a side-effect, not the primary purpose!")
            
            if initial_pos is not None and final_pos is not None:
                print(f"\n  5. Position changed from {initial_pos} → {final_pos}")
                print(f"     (0x{initial_pos:04X} → 0x{final_pos:04X})")
            
            print("=" * 80)
    
    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()