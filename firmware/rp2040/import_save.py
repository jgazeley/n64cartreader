import serial
import struct
import time
import os
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

def import_save(command, input_file):
    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found.")
        return

    with open(input_file, "rb") as f:
        save_data = f.read()
    file_size = len(save_data)
    print(f"File: {input_file} ({file_size} bytes)")

    with serial.Serial(PORT, BAUD, timeout=5) as ser:
        time.sleep(0.5)
        ser.write(b"\r\n")
        time.sleep(0.1)
        ser.reset_input_buffer()

        print(f"Sending {command}...")
        ser.write(f"{command}\r\n".encode())

        expected_size = None
        while True:
            line = ser.readline().decode("ascii", errors="ignore").strip()
            if not line:
                continue
            print(f"Pico: {line}")
            if line.startswith("SAVE_SIZE:"):
                expected_size = int(line.split(":")[1])
            if "ERROR" in line:
                print("Pico reported an error. Aborting.")
                return
            if "READY" in line:
                break

        if expected_size is None:
            print("Never received SAVE_SIZE. Aborting.")
            return

        if file_size != expected_size:
            print(f"Size mismatch: file is {file_size} bytes, "
                  f"cart expects {expected_size} bytes. Aborting.")
            return

        num_chunks = max(1, file_size // CHUNK_SIZE)
        seq        = 0

        print(f"Sending {num_chunks} chunk(s)...")

        for i in range(num_chunks):
            offset     = i * CHUNK_SIZE
            chunk      = save_data[offset : offset + CHUNK_SIZE]
            chunk_crc  = calc_crc16(chunk)

            header = struct.pack('>BBBH', 0xAA, 0x55, seq, len(chunk))
            footer = struct.pack('>H', chunk_crc)
            packet = header + chunk + footer

            retries = 0
            while retries < 5:
                ser.write(packet)
                ser.flush()
                response = ser.read(1)
                if response == b'\x06':
                    time.sleep(0.05)
                    break
                elif response == b'\x15':
                    retries += 1
                    time.sleep(0.05)
                else:
                    retries += 1

            if retries >= 5:
                print(f"\n[!] ABORT: Failed at chunk {i+1}.")
                return

            seq = (seq + 1) & 0xFF
            print(f"\rChunk {i+1}/{num_chunks}", end="")
            sys.stdout.flush()
            time.sleep(0.005)

        print(f"\n\nAll chunks sent.")

        ser.timeout = 2
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
        print("Usage: python import_save.py <command> <input_file>")
        print("  e.g: python import_save.py n64_import_sram  save.srm")
        print("       python import_save.py n64_import_eeprom save.eep")
        sys.exit(1)
    import_save(sys.argv[1], sys.argv[2])

# Usage summary:

# python export_save.py n64_export_sram   goldeneye.srm
# python export_save.py n64_export_eeprom zelda.eep
# python import_save.py n64_import_sram   goldeneye.srm
# python import_save.py n64_import_eeprom zelda.eep