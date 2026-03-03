import serial
import struct
import time
import sys

PORT  = "/dev/ttyACM0"
BAUD  = 115200
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

def dump_rom(output_file="dump.z64"):
    with serial.Serial(PORT, BAUD, timeout=5) as ser:
        time.sleep(0.5)
        ser.write(b"\r\n")
        time.sleep(0.1)
        ser.reset_input_buffer()

        print("Sending n64_dump_rom command...")
        ser.write(b"n64_dump_rom\r\n")

        # Read text lines until we get ROM_SIZE and READY
        rom_size = None
        while True:
            line = ser.readline().decode("ascii", errors="ignore").strip()
            if not line:
                continue
            print(f"Pico: {line}")
            if line.startswith("ROM_SIZE:"):
                rom_size = int(line.split(":")[1])
            if "ERROR" in line:
                print("Pico reported an error. Aborting.")
                return
            if "READY" in line:
                break

        if rom_size is None:
            print("Never received ROM_SIZE. Aborting.")
            return

        num_chunks = rom_size // CHUNK_SIZE
        print(f"ROM size: {rom_size} bytes ({rom_size // 1024} KB), {num_chunks} chunks")

        full_data  = bytearray()
        expected_seq = 0       # wraps at 256, matches Pico uint8_t
        chunks_received = 0    # separate total counter

        while chunks_received < num_chunks:
            # Hunt for sync bytes
            b1 = ser.read(1)
            if not b1:
                print("\n[!] Timeout waiting for data.")
                return
            if b1 != b'\xAA':
                continue
            b2 = ser.read(1)
            if b2 != b'\x55':
                continue

            # Read header: seq (1) + length (2)
            header = ser.read(3)
            if len(header) < 3:
                print("\n[!] Timeout reading header.")
                return
            seq        = header[0]
            payload_len = struct.unpack('>H', header[1:3])[0]

            # Read payload and CRC
            payload = ser.read(payload_len)
            crc_bytes = ser.read(2)
            if len(payload) < payload_len or len(crc_bytes) < 2:
                print("\n[!] Timeout reading payload.")
                ser.write(b'\x15')  # NAK
                continue
            received_crc = struct.unpack('>H', crc_bytes)[0]

            # Verify CRC and sequence
            if received_crc != calc_crc16(payload):
                ser.write(b'\x15')  # NAK - bad CRC
                continue

            if seq == expected_seq:
                full_data.extend(payload)
                ser.write(b'\x06')  # ACK
                expected_seq = (expected_seq + 1) & 0xFF  # wrap at 256
                chunks_received += 1
                sys.stdout.write(
                    f"\rReceived {chunks_received}/{num_chunks} "
                    f"({chunks_received * CHUNK_SIZE // 1024} KB / "
                    f"{rom_size // 1024} KB)"
                )
                sys.stdout.flush()
            elif seq == (expected_seq - 1) & 0xFF:
                # Duplicate — our last ACK was lost, Pico resent
                ser.write(b'\x06')  # ACK again, don't store
            else:
                ser.write(b'\x15')  # NAK - unexpected seq

        print(f"\n\nDump complete. {len(full_data)} bytes received.")
        with open(output_file, "wb") as f:
            f.write(full_data)
        print(f"Saved to {output_file}")

        # Drain any trailing text (DUMP_COMPLETE etc.)
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
    out = sys.argv[1] if len(sys.argv) > 1 else "dump.z64"
    dump_rom(out)