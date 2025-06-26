import serial
import time
import sys

# --- Configuration ---
PORT = "COM28"               # <--- CHANGE THIS to your Pico's COM port
BAUD = 115200
SIZE = 1 * 1024 * 1024       # 1 MiB
OUTPUT_FILENAME = "pico_dump.bin"

def main():
    """Main function to connect, receive, save, and verify data."""
    ser = None  # Initialize ser to None
    try:
        # --- 1. Connect to Pico ---
        print(f"Connecting to {PORT}...")
        ser = serial.Serial(PORT, BAUD, timeout=2.0) # 2-second timeout for reads
        time.sleep(0.2)  # Give the port a moment to settle
        ser.reset_input_buffer()

        # --- 2. Trigger Stream and Wait for Handshake ---
        print("Triggering data stream with 'G'...")
        ser.write(b'G')

        # Wait for the "BEGIN" line from the Pico
        line = ser.readline()
        if line.strip() != b"BEGIN 1MiB":
            print(f"❌ ERROR: Invalid handshake. Expected b'BEGIN 1MiB', but got: {line.strip()}")
            return

        # --- 3. Receive Data ---
        print(f"Receiving {SIZE / (1024*1024):.0f} MiB of data...")
        buf = bytearray()
        start_time = time.time()

        while len(buf) < SIZE:
            # Read a chunk of data. The timeout will trigger if the Pico stalls.
            chunk = ser.read(16384) 
            if not chunk:
                print(f"\n❌ ERROR: Timed out waiting for data. Received {len(buf)} bytes.")
                return
            buf.extend(chunk)
            # Print a dot for every ~16KB received to show progress
            print(".", end="", flush=True)

        elapsed = time.time() - start_time
        print("\nDownload complete.")

        # Optional: Read the trailing "END" message
        # You might need to adjust the sleep/read logic if the END message is missed
        # trailer = ser.read(100) 
        # print(f"Trailer received: {trailer.strip()}")

        # --- 4. Save Data to File ---
        print(f"Saving {len(buf)} bytes to '{OUTPUT_FILENAME}'...")
        with open(OUTPUT_FILENAME, 'wb') as f:
            f.write(buf)
        print(f"File saved successfully. You can now inspect it with a hex editor.")

        # --- 5. Verify Data Integrity ---
        print("Verifying data content...")
        error_found_at = -1
        for i in range(len(buf)):
            expected_byte = i & 0xFF  # The expected value is the offset modulo 256
            actual_byte = buf[i]
            if actual_byte != expected_byte:
                error_found_at = i
                break # Stop at the first error

        print("-" * 40)
        if error_found_at != -1:
            expected = error_found_at & 0xFF
            received = buf[error_found_at]
            print(f"❌ DATA VERIFICATION FAILED!")
            print(f"   Mismatch at offset: {error_found_at:#08x} ({error_found_at})")
            print(f"   Expected value:     {expected:#04x} ({expected})")
            print(f"   Received value:     {received:#04x} ({received})")
        else:
            print("✅ DATA VERIFICATION SUCCESSFUL!")
            mb = len(buf) / (1024 * 1024)
            spd = mb / elapsed
            print(f"   Received {mb:.2f} MiB in {elapsed:.2f}s -> {spd:.2f} MiB/s")
        print("-" * 40)

    except serial.SerialException as e:
        print(f"\nSERIAL ERROR: {e}")
        print(f"Could not open port {PORT}. Please check the port name and ensure no other program is using it.")
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
    finally:
        if ser and ser.is_open:
            ser.close()
        print("Script finished.")

if __name__ == "__main__":
    main()