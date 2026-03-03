import serial
import struct
import time
import sys

PORT       = "/dev/ttyACM0"
BAUD       = 115200
CHUNK_SIZE = 512

def calc_crc16(data: bytes) -> int:
    crc = 0x0000
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x8005) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def export_save(command, output_file):
    with serial.Serial(PORT, BAUD, timeout=5) as ser:
        time.sleep(0.5)
        ser.write(b"\r\n")
        time.sleep(0.1)
        ser.reset_input_buffer()

        print(f"Sending {command}...")
        ser.write(f"{command}\r\n".encode())

        save_type = None
        save_size = None
        while True:
            line = ser.readline().decode("ascii", errors="ignore").strip()
            if not line:
                continue
            print(f"Pico: {line}")
            if line.startswith("SAVE_TYPE:"):
                save_type = line.split(":")[1]
            if line.startswith("SAVE_SIZE:"):
                save_size = int(line.split(":")[1])
            if "ERROR" in line:
                print("Pico reported an error. Aborting.")
                return
            if "READY" in line:
                break

        if save_size is None:
            print("Never received SAVE_SIZE. Aborting.")
            return

        num_chunks    = max(1, save_size // CHUNK_SIZE)
        full_data     = bytearray()
        expected_seq  = 0
        chunks_received = 0

        print(f"Save type: {save_type}, size: {save_size} bytes, {num_chunks} chunk(s)")

        while chunks_received < num_chunks:
            b1 = ser.read(1)
            if not b1:
                print("\n[!] Timeout.")
                return
            if b1 != b'\xAA':
                continue
            b2 = ser.read(1)
            if b2 != b'\x55':
                continue

            header = ser.read(3)
            if len(header) < 3:
                print("\n[!] Timeout reading header.")
                return
            seq         = header[0]
            payload_len = struct.unpack('>H', header[1:3])[0]

            payload   = ser.read(payload_len)
            crc_bytes = ser.read(2)
            if len(payload) < payload_len or len(crc_bytes) < 2:
                ser.write(b'\x15')
                continue
            received_crc = struct.unpack('>H', crc_bytes)[0]

            if received_crc != calc_crc16(payload):
                ser.write(b'\x15')
                continue

            if seq == expected_seq:
                full_data.extend(payload)
                ser.write(b'\x06')
                expected_seq  = (expected_seq + 1) & 0xFF
                chunks_received += 1
                print(f"\rChunk {chunks_received}/{num_chunks}", end="")
                sys.stdout.flush()
            elif seq == (expected_seq - 1) & 0xFF:
                ser.write(b'\x06')  # duplicate, re-ACK
            else:
                ser.write(b'\x15')

        print(f"\n\nExport complete. {len(full_data)} bytes received.")
        with open(output_file, "wb") as f:
            f.write(full_data)
        print(f"Saved to {output_file}")

        ser.timeout = 1
        while True:
            line = ser.readline().decode("ascii", errors="ignore").strip()
            if not line:
                break
            # Extract known keywords even if line contains binary garbage
            for keyword in ["EXPORT_COMPLETE", "IMPORT_COMPLETE", "DUMP_COMPLETE", "ERROR"]:
                if keyword in line:
                    print(f"Pico: {keyword}")
                    break

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python export_save.py <command> <output_file>")
        print("  e.g: python export_save.py n64_export_sram  save.srm")
        print("       python export_save.py n64_export_eeprom save.eep")
        sys.exit(1)
    export_save(sys.argv[1], sys.argv[2])