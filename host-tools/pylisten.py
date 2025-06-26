# import serial
# import sys

# # --- Configuration ---
# PORT = "COM28"              # <--- CHANGE THIS to your Pico's COM port
# BAUD = 115200
# BYTES_TO_READ = 512         # How many bytes to read from the stream
# BYTES_PER_LINE = 16         # For the hex dump format
# OUTPUT_FILENAME = "pico_stream_512.bin"

# def format_hex_dump(data, base_address=0):
#     """
#     Creates a formatted string similar to a hex editor view.
#     """
#     result = []
#     for i in range(0, len(data), BYTES_PER_LINE):
#         chunk = data[i:i + BYTES_PER_LINE]
        
#         # Address part
#         address_str = f"{base_address + i:08X}: "
        
#         # Hex part
#         hex_str = ' '.join(f"{b:02X}" for b in chunk)
#         hex_str = hex_str.ljust(BYTES_PER_LINE * 3 - 1) # Pad for alignment
        
#         # ASCII part
#         ascii_str = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
        
#         result.append(f"{address_str}{hex_str}  |{ascii_str}|")
        
#     return "\n".join(result)

# def main():
#     """Connects, reads 512 bytes, saves to file, and prints hex dump."""
#     ser = None
#     try:
#         print(f"Connecting to {PORT}...")
#         ser = serial.Serial(PORT, BAUD, timeout=2.0)
        
#         print("Flushing buffers and triggering Pico with 'G'...")
#         ser.reset_input_buffer()
#         ser.write(b'G')
        
#         # --- NEW: Handshake Logic ---
#         # First, read and discard any lines of text until we receive the
#         # "BEGIN_DATA" signal from the Pico.
#         print("Waiting for BEGIN_DATA signal from Pico...")
#         while True:
#             try:
#                 line = ser.readline()
#                 if not line:
#                     print("\nError: Timed out waiting for BEGIN_DATA signal from Pico.")
#                     return
                
#                 # Print the line for debugging, so you can see the Pico's printf messages
#                 print(f"PICO > {line.decode('utf-8', errors='ignore').strip()}")

#                 # Check if we got the signal
#                 if line.strip() == b"BEGIN_DATA":
#                     print("BEGIN_DATA signal received. Starting raw data read.")
#                     break
#             except serial.SerialTimeoutException:
#                 print("\nError: Timed out while waiting for signal.")
#                 return

#         # --- Original Read Logic ---
#         print(f"Attempting to read {BYTES_TO_READ} bytes from the stream...")
#         data_buffer = ser.read(BYTES_TO_READ)
        
#         if len(data_buffer) < BYTES_TO_READ:
#             print(f"\nWarning: Read incomplete. Expected {BYTES_TO_READ} bytes, but got {len(data_buffer)}.")
#             if not data_buffer:
#                 return

#         print(f"\nSuccessfully read {len(data_buffer)} bytes.")

#         # Save the raw bytes to a file
#         try:
#             with open(OUTPUT_FILENAME, "wb") as f:
#                 f.write(data_buffer)
#             print(f"Raw data saved to '{OUTPUT_FILENAME}'")
#         except IOError as e:
#             print(f"\nError saving file: {e}")

#         # Print the formatted hex dump
#         print("\n--- Hex Dump ---")
#         rom_base_address = 0x10000000 
#         print(format_hex_dump(data_buffer, rom_base_address))
#         print("----------------\n")

#     except serial.SerialException as e:
#         print(f"\nSERIAL ERROR: {e}")
#         print("Please check the COM port and ensure it's not in use.")
#     except KeyboardInterrupt:
#         print("\nOperation cancelled.")
#     finally:
#         if ser and ser.is_open:
#             ser.close()
#         print("Script finished.")

# if __name__ == "__main__":
#     main()




#!/usr/bin/env python3
import serial
import time
import sys

# —— CONFIG —— 
PORT      = "COM28"            # ← change to your Pico’s CDC port
BAUD      = 115200            # USB CDC ignores this but must be set
TOTAL     = 1 * 1024 * 1024   # 1 MiB
HANDSHAKE = b'G'
TIMEOUT   = 10                # seconds

def main():
    try:
        with serial.Serial(PORT, BAUD, timeout=TIMEOUT) as ser:
            # Give the USB CDC line a moment to settle
            time.sleep(1.0)
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            print("→ Sending handshake 'G' …")
            ser.write(HANDSHAKE)

            print(f"← Reading {TOTAL} bytes …")
            start = time.time()
            data = ser.read(TOTAL)
            duration = time.time() - start
            if duration <= 0:
                duration = 0.0001
            rate = TOTAL / duration

            print(f"→ Transfer took {duration:.2f}s ({rate/1024:.1f} KiB/s)")

            if len(data) != TOTAL:
                print(f"❌ ERROR: received {len(data)} bytes, expected {TOTAL}")
                sys.exit(1)

            print("✔ Received full payload, verifying pattern…")
            for i, byte in enumerate(data):
                expected = i & 0xFF
                if byte != expected:
                    print(f"❌ Mismatch at byte {i}: got 0x{byte:02X}, want 0x{expected:02X}")
                    sys.exit(1)

            print("🎉 Pattern OK!")
    except serial.SerialException as e:
        print(f"❌ Couldn’t open {PORT}: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
