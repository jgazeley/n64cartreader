from __future__ import annotations

import os
import struct
import sys
import time
from dataclasses import dataclass

import serial.tools.list_ports  # type: ignore

from .protocol import (
    ACK,
    CMD_MAGIC,
    DEFAULT_BAUD,
    DEFAULT_CHUNK,
    DEFAULT_PORT,
    DEFAULT_RETRIES,
    DEFAULT_TIMEOUT,
    MagicCliError,
    NAK,
    RSP_MAGIC,
    ST_OK,
    SYNC_A,
    SYNC_B,
    crc16,
    crc16_update,
    require_ok,
)

try:
    import serial  # type: ignore
except ModuleNotFoundError:  # pragma: no cover
    serial = None


def auto_discover_port() -> str:
    """
    Scans for an RP2040 (Raspberry Pi Pico) by VID/PID.
    Returns the port name if found, otherwise returns the DEFAULT_PORT.
    """
    # 2E8A is Raspberry Pi VID, 0005 is the default Pico USB Serial PID
    PICO_VID = 0x2E8A
    PICO_PID = 0x0005

    if serial is None:
        return DEFAULT_PORT

    ports = serial.tools.list_ports.comports()
    for p in ports:
        if p.vid == PICO_VID and p.pid == PICO_PID:
            return p.device
    return DEFAULT_PORT


@dataclass
class Cfg:
    port: str = ""  # Initialized in __post_init__
    baud: int = DEFAULT_BAUD
    timeout: float = DEFAULT_TIMEOUT
    chunk_size: int = DEFAULT_CHUNK
    retries: int = DEFAULT_RETRIES
    quiet: bool = False
    tx_inter_chunk_delay_s: float = 0.0

    def __post_init__(self) -> None:
        if not self.port:
            self.port = auto_discover_port()


class MagicSession:
    def __init__(self, cfg: Cfg) -> None:
        self.cfg = cfg
        self.ser = None

    def __enter__(self) -> "MagicSession":
        if serial is None:
            raise MagicCliError("pyserial missing. Run: pip install -r requirements.txt")
        self.ser = serial.Serial(self.cfg.port, self.cfg.baud, timeout=self.cfg.timeout)
        time.sleep(0.4)
        self.ser.reset_input_buffer()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.ser is not None:
            self.ser.close()

    def _read_exact(self, n: int) -> bytes:
        data = self.ser.read(n)
        if len(data) != n:
            raise MagicCliError(f"Timeout while reading {n} bytes")
        return data

    def _progress(self, label: str, done: int, total: int) -> None:
        if self.cfg.quiet:
            return

        width = 40
        percent = (done / total)
        filled = int(width * percent)
        bar = "#" * filled + "-" * (width - filled)

        # Add a little extra info: label and percentage
        sys.stdout.write(f"\r{label}: [{bar}] {percent:4.0%} ({done}/{total})")
        sys.stdout.flush()

    def _progress_done(self) -> None:
        if not self.cfg.quiet:
            sys.stdout.write("\n")
            sys.stdout.flush()

    def send_cmd(self, cmd: int, arg0: int = 0, arg1: int = 0) -> None:
        if not (0 <= cmd <= 0xFF and 0 <= arg0 <= 0xFF and 0 <= arg1 <= 0xFFFF):
            raise MagicCliError("Command args out of range")
        if os.getenv("PPK_TRACE_CMDS"):
            print(
                f"[TRACE] TX CMD=0x{cmd:02X} arg0=0x{arg0:02X} arg1=0x{arg1:04X}",
                file=sys.stderr,
                flush=True,
            )
        frame = CMD_MAGIC + bytes([cmd, arg0]) + struct.pack(">H", arg1)
        self.ser.write(frame)
        self.ser.flush()

    def recv_rsp(self, expect_cmd: int | None = None) -> tuple[int, int, int]:
        while True:
            b = self.ser.read(1)
            if not b:
                raise MagicCliError("Timeout waiting for response magic")
            if b != RSP_MAGIC[:1]:
                continue

            frame = b + self._read_exact(7)
            if frame[:4] != RSP_MAGIC:
                continue

            cmd = frame[4]
            status = frame[5]
            value = struct.unpack(">H", frame[6:8])[0]
            if os.getenv("PPK_TRACE_CMDS"):
                print(
                    f"[TRACE] RX CMD=0x{cmd:02X} status=0x{status:02X} value=0x{value:04X}",
                    file=sys.stderr,
                    flush=True,
                )
            if expect_cmd is not None and cmd != expect_cmd:
                raise MagicCliError(
                    f"Response cmd mismatch: got 0x{cmd:02X}, expected 0x{expect_cmd:02X}"
                )
            return cmd, status, value

    def send_reliable(self, payload: bytes) -> None:
        seq = 0
        total = len(payload)
        total_chunks = (total + self.cfg.chunk_size - 1) // self.cfg.chunk_size
        for idx in range(total_chunks):
            start = idx * self.cfg.chunk_size
            end = min(start + self.cfg.chunk_size, total)
            chunk = payload[start:end]
            frame = (
                bytes([SYNC_A, SYNC_B, seq])
                + struct.pack(">H", len(chunk))
                + chunk
                + struct.pack(">H", crc16(chunk))
            )

            tries = 0
            while tries < self.cfg.retries:
                self.ser.write(frame)
                self.ser.flush()
                resp = self.ser.read(1)
                if resp == ACK:
                    break
                if resp == RSP_MAGIC[:1]:
                    tail = self.ser.read(7)
                    if len(tail) == 7:
                        full = resp + tail
                        if full[:4] == RSP_MAGIC:
                            cmd = full[4]
                            status = full[5]
                            value = struct.unpack(">H", full[6:8])[0]
                            if status != ST_OK:
                                require_ok(cmd, status, value)
                            raise MagicCliError(
                                f"Unexpected response 0x{cmd:02X}/0x{status:02X} while sending data stream"
                            )
                tries += 1
                time.sleep(0.04)

            if tries >= self.cfg.retries:
                raise MagicCliError(f"Chunk {idx + 1}/{total_chunks} failed after retries")

            seq = (seq + 1) & 0xFF
            self._progress("TX", idx + 1, total_chunks)
            if self.cfg.tx_inter_chunk_delay_s > 0:
                time.sleep(self.cfg.tx_inter_chunk_delay_s)
        self._progress_done()

    def recv_reliable(self, total_size: int) -> bytes:
        expected_seq = 0
        out = bytearray()
        total_chunks = (total_size + self.cfg.chunk_size - 1) // self.cfg.chunk_size
        chunks_received = 0

        while len(out) < total_size:
            b1 = self.ser.read(1)
            if not b1:
                raise MagicCliError("Timeout waiting for packet sync")
            if b1 == RSP_MAGIC[:1]:
                tail = self.ser.read(7)
                if len(tail) == 7:
                    frame = b1 + tail
                    if frame[:4] == RSP_MAGIC:
                        cmd = frame[4]
                        status = frame[5]
                        value = struct.unpack(">H", frame[6:8])[0]
                        if status != ST_OK:
                            require_ok(cmd, status, value)
                        raise MagicCliError(
                            f"Unexpected response 0x{cmd:02X}/0x{status:02X} while waiting for data stream"
                        )
            if b1[0] != SYNC_A:
                continue
            b2 = self.ser.read(1)
            if b2 != bytes([SYNC_B]):
                continue

            header = self.ser.read(3)
            if len(header) < 3:
                self.ser.write(NAK)
                continue
            seq = header[0]
            payload_len = struct.unpack(">H", header[1:3])[0]
            payload = self.ser.read(payload_len)
            crc_bytes = self.ser.read(2)
            if len(payload) < payload_len or len(crc_bytes) < 2:
                self.ser.write(NAK)
                continue

            rx_crc = struct.unpack(">H", crc_bytes)[0]
            if rx_crc != crc16(payload):
                self.ser.write(NAK)
                continue

            if seq == expected_seq:
                remain = total_size - len(out)
                out.extend(payload[:remain])
                self.ser.write(ACK)
                expected_seq = (expected_seq + 1) & 0xFF
                chunks_received += 1
                self._progress("RX", min(chunks_received, total_chunks), total_chunks)
            elif seq == ((expected_seq - 1) & 0xFF):
                self.ser.write(ACK)
            else:
                self.ser.write(NAK)

        self._progress_done()
        return bytes(out)

    def recv_reliable_to_file(self, total_size: int, output_file: str) -> int:
        expected_seq = 0
        written = 0
        total_chunks = (total_size + self.cfg.chunk_size - 1) // self.cfg.chunk_size
        chunks_received = 0
        running_crc = 0

        with open(output_file, "wb") as f:
            while written < total_size:
                b1 = self.ser.read(1)
                if not b1:
                    raise MagicCliError("Timeout waiting for packet sync")
                if b1 == RSP_MAGIC[:1]:
                    tail = self.ser.read(7)
                    if len(tail) == 7:
                        frame = b1 + tail
                        if frame[:4] == RSP_MAGIC:
                            cmd = frame[4]
                            status = frame[5]
                            value = struct.unpack(">H", frame[6:8])[0]
                            if status != ST_OK:
                                require_ok(cmd, status, value)
                            raise MagicCliError(
                                f"Unexpected response 0x{cmd:02X}/0x{status:02X} while waiting for data stream"
                            )
                if b1[0] != SYNC_A:
                    continue
                b2 = self.ser.read(1)
                if b2 != bytes([SYNC_B]):
                    continue

                header = self.ser.read(3)
                if len(header) < 3:
                    self.ser.write(NAK)
                    continue
                seq = header[0]
                payload_len = struct.unpack(">H", header[1:3])[0]
                payload = self.ser.read(payload_len)
                crc_bytes = self.ser.read(2)
                if len(payload) < payload_len or len(crc_bytes) < 2:
                    self.ser.write(NAK)
                    continue

                rx_crc = struct.unpack(">H", crc_bytes)[0]
                if rx_crc != crc16(payload):
                    self.ser.write(NAK)
                    continue

                if seq == expected_seq:
                    remain = total_size - written
                    block = payload[:remain]
                    f.write(block)
                    running_crc = crc16_update(running_crc, block)
                    written += len(block)
                    self.ser.write(ACK)
                    expected_seq = (expected_seq + 1) & 0xFF
                    chunks_received += 1
                    self._progress("RX", chunks_received, total_chunks)
                elif seq == ((expected_seq - 1) & 0xFF):
                    self.ser.write(ACK)
                else:
                    self.ser.write(NAK)

        self._progress_done()
        return running_crc
