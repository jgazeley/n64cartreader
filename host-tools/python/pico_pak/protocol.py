from __future__ import annotations

SYNC_A = 0xAA
SYNC_B = 0x55
ACK = b"\x06"
NAK = b"\x15"
CMD_MAGIC = b"PPK1"
RSP_MAGIC = b"RPK1"

ST_OK = 0x00
ST_BAD_CMD = 0x01
ST_IO_ERR = 0x02
ST_BAD_ARG = 0x03

CMD_PING = 0x01
CMD_GET_INFO = 0x02
CMD_GET_VERSION = 0x03
CMD_FILLBUF = 0x10
CMD_CLEARBUF = 0x11
CMD_WRITEMSG = 0x12
CMD_SWAP16 = 0x13
CMD_SWAP32 = 0x14
CMD_CRC16 = 0x15
CMD_EXPORTBUF = 0x20
CMD_IMPORTBUF = 0x21
CMD_STREAM32K = 0x30
CMD_IMPORT32K = 0x31
CMD_N64_RESCAN = 0x40
CMD_N64_STATUS = 0x41
CMD_N64_EXPORT_SAVE = 0x42
CMD_N64_IMPORT_SAVE = 0x43
CMD_N64_ROM_INFO = 0x44
CMD_N64_EXPORT_ROM = 0x45
CMD_N64_EXPORT_HEADER = 0x46
CMD_N64_CONTROLLER_PROBE = 0x47
CMD_N64_CONTROLLER_POLL = 0x48
CMD_N64_EXPORT_MPK = 0x49
CMD_N64_IMPORT_MPK = 0x4A
CMD_N64_GS_INFO    = 0x4B
CMD_N64_GS_EXPORT  = 0x4C
CMD_N64_EXPORT_SRAM_RAW = 0x4D
CMD_N64_GS_IMPORT      = 0x4E
CMD_N64_SET_SAVE_CFG   = 0x4F
CMD_N64_SET_ROM_SIZE   = 0x50
CMD_N64_ROM_FLASH_ID   = 0x51
CMD_N64_ROM_FLASH_ERASE = 0x52
CMD_N64_ROM_FLASH_WRITE_TEST = 0x53
CMD_N64_REPRO_READ_WORD = 0x54
CMD_N64_REPRO_EEPROM_DIAG = 0x55
CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM = 0x63

# Permanent host-directed configuration commands.
N64_HOST_CONFIG_COMMANDS = {
    CMD_N64_SET_SAVE_CFG,
    CMD_N64_SET_ROM_SIZE,
}

# Experimental repro-cart ROM/NOR lab commands.
N64_REPRO_LAB_COMMANDS = {
    CMD_N64_ROM_FLASH_ID,
    CMD_N64_ROM_FLASH_ERASE,
    CMD_N64_ROM_FLASH_WRITE_TEST,
    CMD_N64_REPRO_READ_WORD,
    CMD_N64_REPRO_EEPROM_DIAG,
    CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM,
}

DEFAULT_PORT = ""
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT = 5.0
DEFAULT_CHUNK = 512
DEFAULT_RETRIES = 5

N64_SAVE_TYPE_NAMES = {
    0: "none",
    1: "sram",
    2: "flashram",
    3: "eeprom4k",
    4: "eeprom16k",
    5: "unknown",
}

N64_ROM_BASE = 0x10000000
N64_SRAM_BASE = 0x08000000
N64_HEADER_SIZE = 64


class MagicCliError(RuntimeError):
    pass


def crc16_update(crc: int, data: bytes) -> int:
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x8005) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def crc16(data: bytes) -> int:
    return crc16_update(0, data)


def require_ok(cmd: int, status: int, value: int) -> int:
    if status == ST_OK:
        return value
    if status == ST_BAD_CMD and cmd in (
        CMD_N64_ROM_INFO,
        CMD_N64_EXPORT_ROM,
        CMD_N64_EXPORT_HEADER,
        CMD_N64_CONTROLLER_PROBE,
        CMD_N64_CONTROLLER_POLL,
        CMD_N64_EXPORT_MPK,
        CMD_N64_IMPORT_MPK,
        CMD_GET_VERSION,
        CMD_N64_GS_INFO,
        CMD_N64_GS_EXPORT,
        CMD_N64_GS_IMPORT,
        CMD_N64_EXPORT_SRAM_RAW,
        CMD_N64_SET_SAVE_CFG,
        CMD_N64_SET_ROM_SIZE,
        CMD_N64_ROM_FLASH_ID,
        CMD_N64_ROM_FLASH_ERASE,
        CMD_N64_ROM_FLASH_WRITE_TEST,
        CMD_N64_REPRO_READ_WORD,
        CMD_N64_REPRO_EEPROM_DIAG,
        CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM,
    ):
        scope = "cmd"
        if cmd in N64_REPRO_LAB_COMMANDS:
            scope = "experimental repro cmd"
        elif cmd in N64_HOST_CONFIG_COMMANDS:
            scope = "host-config cmd"
        raise MagicCliError(
            f"Pico does not support {scope} 0x{cmd:02X}. Flash latest n64-headless UF2."
        )
    status_name = {
        ST_BAD_CMD: "BAD_CMD",
        ST_IO_ERR: "IO_ERR",
        ST_BAD_ARG: "BAD_ARG",
    }.get(status, "UNKNOWN")

    if status == ST_IO_ERR:
        if cmd == CMD_N64_EXPORT_MPK:
            if value == 0xE901:
                raise MagicCliError(
                    "MPK export failed: controller/mempak not detected (E901). "
                    "If an EEPROM cart is connected, unplug controller or cart to avoid Joybus contention."
                )
            if (value & 0xFF00) == 0xEA00:
                blk = value & 0x00FF
                raise MagicCliError(
                    f"MPK export failed: mempak read block {blk} failed (EAxx)."
                )
        if cmd == CMD_N64_IMPORT_MPK:
            if value == 0xEB01:
                raise MagicCliError(
                    "MPK import failed: controller/mempak not detected (EB01). "
                    "If an EEPROM cart is connected, unplug controller or cart to avoid Joybus contention."
                )
            if (value & 0xFF00) == 0xEC00:
                chunk = value & 0x00FF
                raise MagicCliError(
                    f"MPK import failed: host->pico chunk {chunk} receive failure (ECxx)."
                )
            if (value & 0xFF00) == 0xED00:
                blk = value & 0x00FF
                raise MagicCliError(
                    f"MPK import failed: mempak write block {blk} failed (EDxx)."
                )
        if cmd == CMD_N64_GS_EXPORT:
            if value == 0xE701:
                raise MagicCliError("GameShark export failed: probe did not find a supported flash device.")
            if value == 0xE702:
                raise MagicCliError("GameShark export failed: invalid/unknown flash size reported.")
            if value == 0xE703:
                raise MagicCliError("GameShark export failed: flash read transaction failed.")
            if value == 0xE704:
                raise MagicCliError("GameShark export failed: transport stream send failed.")
            if value == 0xE706:
                raise MagicCliError("GameShark export failed: odd-length chunk boundary detected.")
        if cmd == CMD_N64_GS_IMPORT:
            if value == 0xE801:
                raise MagicCliError("GameShark import failed: probe did not find a supported flash device.")
            if value == 0xE802:
                raise MagicCliError("GameShark import failed: chip does not support erase/write.")
            if value == 0xE803:
                raise MagicCliError("GameShark import failed: invalid/unknown flash size reported.")
            if value == 0xE804:
                raise MagicCliError("GameShark import failed: erase failed.")
            if value == 0xE805:
                raise MagicCliError("GameShark import failed: write transaction failed.")
            if value == 0xE806:
                raise MagicCliError("GameShark import failed: odd-length chunk boundary detected.")
            if (value & 0xFF00) == 0xE900:
                raise MagicCliError(f"GameShark import failed: host->pico stream receive failed on chunk {value & 0xFF}.")
        if cmd == CMD_N64_EXPORT_SRAM_RAW:
            if value == 0xE801:
                raise MagicCliError("Raw SRAM export failed: no valid cartridge session (init/header failed).")
            if value == 0xE802:
                raise MagicCliError("Raw SRAM export failed: invalid requested length.")
            if value == 0xE803:
                raise MagicCliError("Raw SRAM export failed: SRAM read transaction failed.")
            if value == 0xE804:
                raise MagicCliError("Raw SRAM export failed: transport stream send failed.")
    raise MagicCliError(
        f"Pico returned status 0x{status:02X} ({status_name}) "
        f"for cmd 0x{cmd:02X} (value=0x{value:04X})"
    )


def decode_n64_status(meta: int) -> tuple[int, int]:
    save_type = (meta >> 12) & 0x0F
    size_units_64 = meta & 0x0FFF
    return save_type, size_units_64 * 64


def decode_n64_rom_size(size_units_2k: int) -> int:
    return size_units_2k * 2048
