#!/usr/bin/env python3
"""
Actuator Control and Telemetry Script - Thread-Safe Version
- Implements a mutex (lock) to prevent race conditions between the
  main control thread and the background polling thread.
"""
import serial
import time
import sys
import threading
from dataclasses import dataclass, field

# --- Configuration [Unchanged] ---
CONFIG = {
  "actuator": {
    "port": "/dev/ttyUSB0",
    "baudRate": 115200,
    "screw_lead_in": 0.125,
    "counts_per_rev": 1024,
    "retracted_offset_counts": 2048
  }
}

# --- ActuatorTelemetry Class [Unchanged] ---
@dataclass
class ActuatorTelemetry:
    position_in: float = 0.0
    velocity_in_s: float = 0.0
    torque_a: float = 0.0
    raw_status: str = "N/A"
    is_moving: bool = False
    _last_pos_in: float = field(default=0.0, repr=False)
    _last_time_s: float = field(default=0.0, repr=False)

class ActuatorController:
    """A thread-safe class to control and monitor the actuator."""
    def __init__(self, config):
        self.config = config
        self.ser = None
        self.telemetry = ActuatorTelemetry()
        self._telemetry_lock = threading.Lock()
        self._stop_event = threading.Event()
        self._polling_thread = None
        self.last_command_acknowledged = False
        
        # --- START OF CORRECTION: Add a lock for serial port access ---
        self._serial_lock = threading.Lock()
        # --- END OF CORRECTION ---

    # ... [ _calculate_checksum and _construct_command are unchanged ] ...
    def _calculate_checksum(self, string_to_checksum: str) -> str:
        ascii_sum = sum(ord(char) for char in string_to_checksum)
        return f"{(ascii_sum % 256):02X}"

    def _construct_command(self, command: str, argument: str = "") -> bytes:
        if argument:
            base_string = f"{command} {argument}"
            string_to_checksum = f"{base_string} "
            checksum = self._calculate_checksum(string_to_checksum)
            full_command = f"{base_string} {checksum}\r"
        else:
            string_to_checksum = command
            checksum = self._calculate_checksum(string_to_checksum)
            full_command = f"{command}{checksum}\r"
        return full_command.encode('ascii')

    def _send_and_parse(self, command_str: str):
        # --- START OF CORRECTION: Protect this entire function with the lock ---
        with self._serial_lock:
            if not self.ser or not self.ser.is_open: return None
            
            parts = command_str.split()
            cmd = parts[0]
            arg = " ".join(parts[1:]) if len(parts) > 1 else ""
            
            command_bytes = self._construct_command(cmd, arg)
            
            self.ser.reset_input_buffer()
            self.ser.write(command_bytes)
            response = self.ser.read_until(b'\r').strip().decode()

            if not response.startswith('A'):
                print(f"DEBUG: NACK or unexpected response: '{response}' for command '{command_str}'")
                return None
            
            last_space_index = response.rfind(' ')
            if last_space_index == -1: return None
            
            main_part = response[:last_space_index]
            received_checksum = response[last_space_index+1:]
            string_to_validate = f"{main_part} "
            calculated_checksum = self._calculate_checksum(string_to_validate)

            if received_checksum.upper() == calculated_checksum.upper():
                return main_part[1:].strip()
            
            print(f"DEBUG: Checksum mismatch on response '{response}'")
            return None
        # --- END OF CORRECTION ---

    # ... [ connect, disconnect, _polling_loop, start_polling, get_telemetry are mostly unchanged ] ...
    def connect(self) -> bool:
        try:
            print(f"Connecting to actuator on {self.config['port']}...")
            self.ser = serial.Serial(port=self.config['port'], baudrate=self.config['baudRate'], timeout=0.5,
                                     xonxoff=False, rtscts=False, dsrdtr=False)
            if self._send_and_parse("AP") is not None:
                print("✓ Connection successful!")
                return True
            else: self.ser.close(); return False
        except serial.SerialException as e: print(f"✗ FATAL ERROR: {e}"); return False

    def disconnect(self):
        if self._polling_thread and self._polling_thread.is_alive():
            self._stop_event.set(); self._polling_thread.join()
        if self.ser and self.ser.is_open: self.ser.close()
        print("Cleanup complete. Program exited.")

    def _polling_loop(self):
        # Initialize time and position for the first velocity calculation
        time.sleep(1) # Initial delay to allow main thread to start
        
        while not self._stop_event.is_set():
            # This loop now implicitly uses the lock inside _send_and_parse
            pos_counts_str = self._send_and_parse("AP")
            torq_counts_str = self._send_and_parse("TQ")
            status_hex_str = self._send_and_parse("SR")
            
            with self._telemetry_lock:
                current_time = time.monotonic()
                delta_t = current_time - self.telemetry._last_time_s if self.telemetry._last_time_s > 0 else 0.1
                L, N_cpr, N_offset = self.config['screw_lead_in'], self.config['counts_per_rev'], self.config['retracted_offset_counts']
                
                if pos_counts_str and pos_counts_str.isdigit():
                    pos_counts = int(pos_counts_str)
                    current_pos_in = (pos_counts - N_offset) * L / N_cpr
                    if delta_t > 0.001:
                        self.telemetry.velocity_in_s = (current_pos_in - self.telemetry.position_in) / delta_t
                    self.telemetry.position_in = current_pos_in
                    self.telemetry._last_time_s = current_time

                if torq_counts_str and torq_counts_str.lstrip('-').isdigit():
                    self.telemetry.torque_a = int(torq_counts_str) / 1848.43

                if status_hex_str:
                    self.telemetry.raw_status = "0x" + status_hex_str
                    self.telemetry.is_moving = (int(status_hex_str, 16) >> 6) & 1

            # The polling interval is now the time it takes to run the commands + this sleep
            time.sleep(0.05) # Short sleep, actual rate is dominated by serial timeouts

    def start_polling(self):
        self._stop_event.clear()
        self._polling_thread = threading.Thread(target=self._polling_loop, daemon=True)
        self._polling_thread.start()
        print("Telemetry polling thread started.")

    def get_telemetry(self) -> ActuatorTelemetry:
        with self._telemetry_lock: return self.telemetry

    def move_to_position_by_counts(self, counts: int):
        # This function also implicitly uses the lock via _send_and_parse
        response = self._send_and_parse(f"TA {counts}")
        self.last_command_acknowledged = (response is not None)

# --- [ main() and wait_for_move_completion() are unchanged ] ---
def wait_for_move_completion(actuator, target_counts, timeout=20):
    target_pos_in = (target_counts - actuator.config['retracted_offset_counts']) * actuator.config['screw_lead_in'] / actuator.config['counts_per_rev']
    position_tolerance_in = 0.05
    start_time = time.time()
    while time.time() - start_time < timeout:
        telemetry = actuator.get_telemetry()
        telemetry_str = (
                f"  -> Moving... "
                f"Pos: {telemetry.position_in * 25.4:7.2f} mm | "
                f"Target: {target_pos_in * 25.4:7.2f} mm | "
                f"Vel: {telemetry.velocity_in_s * 25.4:7.2f} mm/s | "
                f"Torque: {telemetry.torque_a:5.2f} A"
)
        sys.stdout.write(telemetry_str + '\n'); sys.stdout.flush()
        if abs(telemetry.position_in - target_pos_in) < position_tolerance_in:
            time.sleep(0.5)
            final_telemetry = actuator.get_telemetry()
            if abs(final_telemetry.velocity_in_s) < 0.1:
                sys.stdout.write('\n'); return True
        time.sleep(0.1)
    sys.stdout.write('\n'); return False

def main():
    actuator = ActuatorController(CONFIG['actuator'])
    if not actuator.connect(): sys.exit(1)
    actuator.start_polling()
    time.sleep(1) # Allow polling to start and get first reading
    try:
        target_counts_1 = 63000; target_counts_2 = actuator.config['retracted_offset_counts']
        print(f"\n>>> STEP 1: Commanding move to {target_counts_1} counts..."); actuator.move_to_position_by_counts(target_counts_1)
        if actuator.last_command_acknowledged:
            print("    -> Move command acknowledged.")
            if wait_for_move_completion(actuator, target_counts_1): print("    -> Arrived at target 1.")
            else: print("    -> ⚠ TIMEOUT waiting to reach target 1.")
        else: print("    -> ⚠ Move command FAILED.")
        print(f"\n>>> STEP 2: Waiting 2s and returning to {target_counts_2} counts..."); time.sleep(2)
        actuator.move_to_position_by_counts(target_counts_2)
        if actuator.last_command_acknowledged:
            print("    -> Move command acknowledged.")
            if wait_for_move_completion(actuator, target_counts_2): print("    -> Arrived at target 2.")
            else: print("    -> ⚠ TIMEOUT waiting to reach target 2.")
        else: print("    -> ⚠ Move command FAILED.")
        print("\n\n>>> Motion sequence complete. <<<")
    except KeyboardInterrupt: print("\n\nUser interrupted. Shutting down.")
    finally: actuator.disconnect()

if __name__ == "__main__":
    main()