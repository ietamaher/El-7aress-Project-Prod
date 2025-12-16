import minimalmodbus
import time
import logging

# Optional logging
logging.basicConfig(level=logging.INFO)
log = logging.getLogger()

# -------------------- SETTINGS --------------------

PORT = "/dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BC046FABCD-if04"
BAUDRATE = 230400
SLAVE_ID = 2
TIMEOUT = 0.5  # seconds

# Create Modbus client (RTU)
instrument = minimalmodbus.Instrument(PORT, SLAVE_ID)
instrument.serial.baudrate = BAUDRATE
instrument.serial.timeout = TIMEOUT
instrument.serial.bytesize = 8
instrument.serial.parity = minimalmodbus.serial.PARITY_NONE
instrument.serial.stopbits = 1
instrument.mode = minimalmodbus.MODE_RTU


# -------------------- HELPERS --------------------

def write_preset_position(value: int):
    """
    Write a 32-bit preset position
    using two consecutive 16-bit registers.
    """
    # Convert 32-bit signed into two 16-bit words
    if value < 0:
        value &= 0xFFFFFFFF

    low_word = value & 0xFFFF
    high_word = (value >> 16) & 0xFFFF

    log.info(f"Writing Preset Position = {value} (Low={low_word}, High={high_word})")

    # Addresses from your manual:
    # 453 = 0x01C5 (lower word)
    # 4549 = 0x11C5 (upper word)
    instrument.write_register(453, low_word)   # preset lower
    instrument.write_register(4549, high_word) # preset upper

    log.info("Preset position written.")


def execute_p_preset_safe(instrument):
    # 1) Read status and confirm motor is stopped & READY ON
    # Driver output/status register where READY is available (read I/O status / R-OUT).
    # The manual maps driver output status bits; check your unit for the correct register if different.
    STATUS_REG = 127   # driver output status (example from manual mapping)
    status = instrument.read_register(STATUS_REG)
    READY_BIT = 5      # READY is R-OUT5 in the manual mapping
    if not (status & (1 << READY_BIT)):
        raise RuntimeError("Driver not READY or motor is operating — stop the motor before P-PRESET per manual.")

    # 2) Write the Preset Position registers (lower, upper) as before
    # (keep your write_preset_position function)

    # 3) Execute P-PRESET by writing 1 to the maintenance command register (lower word)
    P_PRESET_REG = 394  # register for P-PRESET (lower word)
    instrument.write_register(P_PRESET_REG, 1)   # write 1 to execute, per manual
    time.sleep(0.2)
    instrument.write_register(P_PRESET_REG, 0)   # reset it back to 0 (optional cleanup)
    # 4) Optionally verify the preset completed by reading back the feedback/command position
 

def read_status(addr: int):
    s = instrument.read_register(addr)
    log.info(f"Status @ {addr} = {s:#06x}")
    return s


# -------------------- MAIN --------------------

if __name__ == "__main__":
    try:
        # Write preset position (no motion)
        write_preset_position(0)

        time.sleep(0.2)

        # Execute P-PRESET safely
        execute_p_preset_safe(instrument)

    except IOError as e:
        log.error(f"Communication error: {e}")
