from __future__ import annotations

import os
import struct

from .protocol import (
    CMD_GET_VERSION,
    CMD_N64_CONTROLLER_POLL,
    CMD_N64_CONTROLLER_PROBE,
    CMD_N64_EXPORT_HEADER,
    CMD_N64_EXPORT_MPK,
    CMD_N64_EXPORT_ROM,
    CMD_N64_EXPORT_SAVE,
    CMD_N64_EXPORT_SRAM_RAW,
    CMD_N64_GS_INFO,
    CMD_N64_GS_EXPORT,
    CMD_N64_GS_IMPORT,
    CMD_N64_IMPORT_MPK,
    CMD_N64_IMPORT_SAVE,
    CMD_N64_RESCAN,
    CMD_N64_REPRO_READ_WORD,
    CMD_N64_ROM_INFO,
    CMD_N64_ROM_FLASH_ERASE,
    CMD_N64_ROM_FLASH_ID,
    CMD_N64_ROM_FLASH_WRITE_TEST,
    CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM,
    CMD_N64_SET_ROM_SIZE,
    CMD_N64_SET_SAVE_CFG,
    CMD_N64_STATUS,
    ST_OK,
    N64_HEADER_SIZE,
    N64_SAVE_TYPE_NAMES,
    MagicCliError,
    crc16,
    decode_n64_rom_size,
    decode_n64_status,
    require_ok,
)
from .transport import MagicSession

N64_MEMPAK_SIZE = 32768
FW_VERSION_PAYLOAD_SIZE = 64
N64_REPRO_ALIAS_DIAG_OFFSETS = (
    0x000000,
    0x000002,
    0x000800,
    0x001000,
    0x002000,
    0x004000,
    0x008000,
    0x010000,
    0x020000,
    0x040000,
    0x080000,
    0x100000,
    0x200000,
)

N64_BUTTON_A = 1 << 15
N64_BUTTON_B = 1 << 14
N64_BUTTON_Z = 1 << 13
N64_BUTTON_START = 1 << 12
N64_BUTTON_D_UP = 1 << 11
N64_BUTTON_D_DOWN = 1 << 10
N64_BUTTON_D_LEFT = 1 << 9
N64_BUTTON_D_RIGHT = 1 << 8
N64_BUTTON_L = 1 << 5
N64_BUTTON_R = 1 << 4
N64_BUTTON_C_UP = 1 << 3
N64_BUTTON_C_DOWN = 1 << 2
N64_BUTTON_C_LEFT = 1 << 1
N64_BUTTON_C_RIGHT = 1 << 0


def save_type_name(save_type: int) -> str:
    return N64_SAVE_TYPE_NAMES.get(save_type, f"type{save_type}")


def pico_firmware_version(sess: MagicSession) -> str:
    sess.send_cmd(CMD_GET_VERSION)
    data = sess.recv_reliable(FW_VERSION_PAYLOAD_SIZE)
    cmd, st, dev_crc = sess.recv_rsp(CMD_GET_VERSION)
    require_ok(cmd, st, dev_crc)

    host_crc = crc16(data)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after version query: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )

    return data.split(b"\x00", 1)[0].decode("ascii", errors="replace").strip()


def _save_unavailable_message(sess: MagicSession, save_type: int, save_size: int) -> str:
    if save_type == 5 and save_size == 0:
        return (
            "N64 save probe conflict: controller + cartridge EEPROM are responding on the "
            "same Joybus data line. Unplug controller and rescan for EEPROM carts."
        )
    if save_type == 0 and save_size == 0:
        # Check if this is a GameShark — save operations don't apply.
        try:
            from .cart_id import is_gameshark_header
            header, _ = n64_export_header(sess, rescan=False)
            if is_gameshark_header(header):
                return (
                    "GameShark / Action Replay detected — save export/import is not "
                    "applicable. Use n64-gs-export / n64-gs-import for GameShark flash."
                )
        except Exception:
            pass
        # Heuristic: if a controller is present while save probe returns "none",
        # the shared Joybus line can mask EEPROM detection on some setups.
        try:
            device, _status = n64_controller_probe(sess)
            if device != 0:
                return (
                    "No cart save detected (type=none) while controller is present. "
                    "On shared Joybus wiring this can mask EEPROM carts. "
                    "Unplug controller and rescan."
                )
        except Exception:
            pass
    return f"N64 save not available (type={save_type}, size={save_size})"


def n64_rescan(sess: MagicSession) -> None:
    sess.send_cmd(CMD_N64_RESCAN)
    cmd, st, val = sess.recv_rsp(CMD_N64_RESCAN)
    require_ok(cmd, st, val)


def n64_rescan_and_configure(
    sess: MagicSession,
    db_path: str | None = None,
    catalog_txt_path: str | None = None,
) -> dict | None:
    """Rescan the cartridge and re-push save type + ROM size from host catalog data.

    After the host-directed refactoring, a bare rescan wipes the firmware's
    save config. ROM size is still detected by firmware; this helper only falls
    back to host ROM-size override if the detected size is unavailable.

    Returns the identify_cart result dict, or None if identification failed.
    """
    from .cart_id import identify_cart, save_hint_to_fw_config

    # Safety: clear stale save config before rescan. If the previous cart was
    # FlashRAM, the firmware may still have FlashRAM bus state armed. Clearing
    # to NONE ensures no leftover state interacts with the new cart during
    # rescan's adbus_init().
    try:
        n64_set_save_cfg(sess, 0, 0)
    except MagicCliError:
        pass  # OK if this fails (e.g. first connect, no cart yet)

    n64_rescan(sess)

    header, _ = n64_export_header(sess, rescan=False)
    try:
        rom_size, _ = n64_rom_info(sess, rescan=False)
    except MagicCliError:
        rom_size = 0

    result = identify_cart(
        header,
        rom_size,
        db_path=db_path,
        catalog_txt_path=catalog_txt_path,
    )

    # GameShark detected — do not push any save config.
    if result.get("is_gameshark"):
        return result

    matched = result.get("matched")
    if not matched:
        return result

    # Push ROM size
    matched_rom_size = int(matched.get("rom_size") or 0)
    if rom_size <= 0 and matched_rom_size > 0:
        try:
            confirmed = n64_set_rom_size(sess, matched_rom_size)
            result["rom_size_configured"] = confirmed
        except MagicCliError:
            result["rom_size_configured"] = None
    else:
        result["rom_size_configured"] = rom_size if rom_size > 0 else None

    # Push save type
    if matched.get("save_type_hint"):
        cfg = save_hint_to_fw_config(matched["save_type_hint"])
        if cfg is not None:
            fw_type, fw_size = cfg
            try:
                n64_set_save_cfg(sess, fw_type, fw_size)
            except MagicCliError:
                pass

    return result


def n64_status(sess: MagicSession, rescan: bool = False) -> tuple[int, int, int]:
    if rescan:
        n64_rescan_and_configure(sess)
    sess.send_cmd(CMD_N64_STATUS)
    cmd, st, meta = sess.recv_rsp(CMD_N64_STATUS)
    require_ok(cmd, st, meta)
    save_type, save_size = decode_n64_status(meta)
    return save_type, save_size, meta


def n64_set_save_cfg(sess: MagicSession, save_type: int, size_bytes: int) -> tuple[int, int]:
    """Send SET_SAVE_CFG to firmware. Returns (save_type, save_size) as confirmed."""
    if not (0 <= save_type <= 4):
        raise MagicCliError(f"Invalid save_type {save_type}, must be 0-4")
    size_units_64 = (size_bytes + 63) // 64 if size_bytes > 0 else 0
    if size_units_64 > 0xFFFF:
        raise MagicCliError(f"Save size too large: {size_bytes}")
    sess.send_cmd(CMD_N64_SET_SAVE_CFG, arg0=save_type, arg1=size_units_64 & 0xFFFF)
    cmd, st, meta = sess.recv_rsp(CMD_N64_SET_SAVE_CFG)
    require_ok(cmd, st, meta)
    return decode_n64_status(meta)


def n64_set_rom_size(sess: MagicSession, size_bytes: int) -> int:
    """Send SET_ROM_SIZE to firmware. Returns confirmed size in bytes."""
    if size_bytes <= 0 or size_bytes > 64 * 1024 * 1024:
        raise MagicCliError(f"Invalid ROM size: {size_bytes}")
    units_2k = (size_bytes + 2047) // 2048
    if units_2k > 0xFFFF:
        raise MagicCliError(f"ROM size too large for protocol: {size_bytes}")
    sess.send_cmd(CMD_N64_SET_ROM_SIZE, arg1=units_2k & 0xFFFF)
    cmd, st, confirmed_2k = sess.recv_rsp(CMD_N64_SET_ROM_SIZE)
    require_ok(cmd, st, confirmed_2k)
    return decode_n64_rom_size(confirmed_2k)


def n64_rom_flash_id(sess: MagicSession) -> dict:
    """Run the experimental repro NOR autoselect probe.

    Returns a dict with keys: mfg_id, dev_id0, dev_id1, dev_id2 (all int),
    and 'present' (bool — False means open-bus, a mask ROM cart, or an
    unsupported repro implementation).
    """
    sess.send_cmd(CMD_N64_ROM_FLASH_ID)
    # Firmware streams 8 bytes: 4 × uint16 in native RP2040 order (little-endian)
    data = sess.recv_reliable(8)
    cmd, st, mfg_id_val = sess.recv_rsp(CMD_N64_ROM_FLASH_ID)
    present = (st == 0x00)  # ST_OK = chip responded; ST_IO_ERR = open bus

    mfg_id, dev_id0, dev_id1, dev_id2 = struct.unpack("<HHHH", data)
    return {
        "present":  present,
        "mfg_id":   mfg_id,
        "dev_id0":  dev_id0,
        "dev_id1":  dev_id1,
        "dev_id2":  dev_id2,
    }


def n64_rom_flash_erase(sess: MagicSession) -> dict:
    """Run the experimental repro NOR chip erase command.

    Blocks until the erase completes (up to ~10 minutes).
    Returns diagnostic dict with pre/post data and status register values.
    """
    sess.send_cmd(CMD_N64_ROM_FLASH_ERASE)
    # Chip erase can take up to 480 seconds — temporarily bump serial timeout
    old_timeout = sess.ser.timeout
    sess.ser.timeout = 660  # 11 minutes
    try:
        # Firmware streams 12 bytes: 6 × uint16 LE diagnostic struct
        data = sess.recv_reliable(12)
        cmd, st, poll_count = sess.recv_rsp(CMD_N64_ROM_FLASH_ERASE)
    finally:
        sess.ser.timeout = old_timeout

    pre_word, first_sr, final_sr, post_word, count, ok = struct.unpack("<HHHHHH", data)
    return {
        "success": st == 0x00,
        "pre_word": pre_word,
        "first_sr": first_sr,
        "final_sr": final_sr,
        "post_word": post_word,
        "poll_count": count,
        "ok": ok,
    }


def n64_rom_flash_write_test(sess: MagicSession) -> dict:
    """Backward-compatible alias for the repro ROM aliasing diagnostic."""
    return n64_repro_rom_diagnostic(sess)


def n64_repro_rom_diagnostic(sess: MagicSession) -> dict:
    """Run the experimental non-destructive repro ROM aliasing diagnostic.

    The payload is a sequence of sampled 16-bit words read at the fixed offsets
    listed in N64_REPRO_ALIAS_DIAG_OFFSETS.
    """
    sess.send_cmd(CMD_N64_ROM_FLASH_WRITE_TEST)
    # The diagnostic payload is 13 uint16 values = 26 bytes.
    data = sess.recv_reliable(26)
    cmd, st, _ = sess.recv_rsp(CMD_N64_ROM_FLASH_WRITE_TEST)

    words = struct.unpack("<" + "H" * len(N64_REPRO_ALIAS_DIAG_OFFSETS), data)
    samples = {
        offset: word
        for offset, word in zip(N64_REPRO_ALIAS_DIAG_OFFSETS, words)
    }
    return {
        "success": st == 0x00,
        "offsets": list(N64_REPRO_ALIAS_DIAG_OFFSETS),
        "samples": samples,
        "words": list(words),
    }


def n64_repro_read_word(
    sess: MagicSession,
    byte_offset: int,
    *,
    reset_carrier: bool = False,
) -> int:
    """Read one ROM word from a repro cart at an arbitrary even byte offset.

    If reset_carrier=True, send the repro flash-ID command first so counterfeit
    NOR parts are nudged back into read-array mode before the read.
    """
    if byte_offset < 0 or byte_offset > 0x01FFFFFE:
        raise MagicCliError(
            f"Repro read-word offset out of range: 0x{byte_offset:X} "
            "(supported range 0x000000-0x01FFFFFE)"
        )
    if (byte_offset & 1) != 0:
        raise MagicCliError(f"Repro read-word offset must be even, got 0x{byte_offset:X}")

    def _read_once(offset: int) -> int:
        arg0 = (offset >> 17) & 0xFF
        arg1 = offset & 0xFFFE
        sess.send_cmd(CMD_N64_REPRO_READ_WORD, arg0=arg0, arg1=arg1)
        cmd, st, value = sess.recv_rsp(CMD_N64_REPRO_READ_WORD)
        require_ok(cmd, st, value)
        return value

    if reset_carrier:
        # This command is used here only as a read-array reset carrier. The
        # returned flash IDs are not required for the read itself.
        n64_rom_flash_id(sess)
        # The first word read immediately after the reset carrier is often a
        # stale header word on this repro. Throw away one warm-up read at the
        # requested offset before returning the real sample.
        _read_once(byte_offset)

    return _read_once(byte_offset)


def n64_mx29lv640_export_window_stream(sess: MagicSession, byte_offset: int) -> bytes:
    """Export a 512-byte MX29LV640 ROM window using the proven raw stream path."""
    if byte_offset < 0 or byte_offset > 0x007FFE00:
        raise MagicCliError(
            f"MX29LV640 export-window-stream offset out of range: 0x{byte_offset:X}"
        )
    if (byte_offset & 1) != 0:
        raise MagicCliError(
            f"MX29LV640 export-window-stream offset must be even, got 0x{byte_offset:X}"
        )

    word_offset = byte_offset >> 1
    arg0 = (word_offset >> 16) & 0xFF
    arg1 = word_offset & 0xFFFF
    sess.send_cmd(CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM, arg0=arg0, arg1=arg1)
    data = sess.recv_reliable(512)
    cmd, st, _ = sess.recv_rsp(CMD_N64_MX29LV640_EXPORT_WINDOW_STREAM)
    require_ok(cmd, st, 0)
    return data


def n64_rom_info(sess: MagicSession, rescan: bool = False) -> tuple[int, int]:
    if rescan:
        n64_rescan_and_configure(sess)
    sess.send_cmd(CMD_N64_ROM_INFO)
    cmd, st, units2k = sess.recv_rsp(CMD_N64_ROM_INFO)
    require_ok(cmd, st, units2k)
    return decode_n64_rom_size(units2k), units2k


def n64_dump_rom(sess: MagicSession, out_path: str, rescan: bool = False) -> tuple[int, int]:
    rom_size, _ = n64_rom_info(sess, rescan=rescan)
    if rom_size <= 0:
        raise MagicCliError("N64 ROM size reported as 0")

    sess.send_cmd(CMD_N64_EXPORT_ROM)
    host_crc = sess.recv_reliable_to_file(rom_size, out_path)
    cmd, st, dev_crc = sess.recv_rsp(CMD_N64_EXPORT_ROM)
    require_ok(cmd, st, dev_crc)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after ROM dump: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )
    return rom_size, host_crc


def n64_export_header(sess: MagicSession, rescan: bool = False) -> tuple[bytes, int]:
    if rescan:
        n64_rescan_and_configure(sess)

    sess.send_cmd(CMD_N64_EXPORT_HEADER)
    data = sess.recv_reliable(N64_HEADER_SIZE)
    cmd, st, dev_crc = sess.recv_rsp(CMD_N64_EXPORT_HEADER)
    require_ok(cmd, st, dev_crc)

    host_crc = crc16(data)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after header export: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )
    return data, host_crc


def n64_export_save_bytes(sess: MagicSession, rescan: bool = False) -> tuple[int, bytes, int, int]:
    save_type, save_size, _ = n64_status(sess, rescan=rescan)
    if save_size <= 0 or save_type in (0, 5):
        raise MagicCliError(_save_unavailable_message(sess, save_type, save_size))

    sess.send_cmd(CMD_N64_EXPORT_SAVE)
    data = sess.recv_reliable(save_size)
    cmd, st, dev_crc = sess.recv_rsp(CMD_N64_EXPORT_SAVE)
    require_ok(cmd, st, dev_crc)

    host_crc = crc16(data)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after export: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )

    return save_type, data, save_size, host_crc


def n64_export_save(sess: MagicSession, out_path: str, rescan: bool = False) -> tuple[int, int, int, int]:
    save_type, data, save_size, host_crc = n64_export_save_bytes(sess, rescan=rescan)

    with open(out_path, "wb") as f:
        f.write(data)

    return save_type, len(data), save_size, host_crc


def n64_export_sram_raw(
    sess: MagicSession,
    out_path: str,
    size_bytes: int = 32768,
    rescan: bool = False,
) -> tuple[int, int]:
    if rescan:
        n64_rescan_and_configure(sess)

    if size_bytes <= 0 or (size_bytes % 512) != 0:
        raise MagicCliError(f"Raw SRAM export size must be positive and 512-byte aligned, got {size_bytes}")
    if (size_bytes % 2) != 0:
        raise MagicCliError(f"Raw SRAM export size must be even, got {size_bytes}")

    chunk_count = size_bytes // 512
    if chunk_count > 0xFFFF:
        raise MagicCliError(f"Raw SRAM export size too large for protocol: {size_bytes}")

    sess.send_cmd(CMD_N64_EXPORT_SRAM_RAW, arg1=chunk_count & 0xFFFF)
    data = sess.recv_reliable(size_bytes)
    cmd, st, dev_crc = sess.recv_rsp(CMD_N64_EXPORT_SRAM_RAW)
    require_ok(cmd, st, dev_crc)

    host_crc = crc16(data)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after raw SRAM export: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )

    with open(out_path, "wb") as f:
        f.write(data)
    return size_bytes, host_crc


def n64_import_save(sess: MagicSession, in_path: str, rescan: bool = False, verify: bool = False) -> tuple[int, int]:
    if not os.path.exists(in_path):
        raise MagicCliError(f"Input file not found: {in_path}")

    with open(in_path, "rb") as f:
        data = f.read()

    save_type, save_size, _ = n64_status(sess, rescan=rescan)
    if save_size <= 0 or save_type in (0, 5):
        raise MagicCliError(_save_unavailable_message(sess, save_type, save_size))
    if len(data) != save_size:
        raise MagicCliError(f"Size mismatch: file={len(data)} bytes, cart expects {save_size} bytes")

    old_timeout = sess.ser.timeout
    old_retries = sess.cfg.retries
    old_delay = sess.cfg.tx_inter_chunk_delay_s

    # FlashRAM (Type 2) is notoriously slow due to 128KB erase cycles.
    # We must be extremely patient to avoid USB timeouts.
    if save_type == 2:
        sess.ser.timeout = max(float(old_timeout), 15.0)
        sess.cfg.retries = max(int(old_retries), 10)
        sess.cfg.tx_inter_chunk_delay_s = max(float(old_delay), 0.15) # 150ms between chunks

    try:
        sess.send_cmd(CMD_N64_IMPORT_SAVE)
        sess.send_reliable(data)
        cmd, st, dev_crc = sess.recv_rsp(CMD_N64_IMPORT_SAVE)
        require_ok(cmd, st, dev_crc)

        host_crc = crc16(data)
        if host_crc != dev_crc:
            raise MagicCliError(
                f"CRC mismatch after import: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
            )

        if verify:
            print(f"\nVerifying import (reading back {save_size} bytes)...")
            _st, verify_data, _sz, verify_crc = n64_export_save_bytes(sess, rescan=False)
            if verify_crc != host_crc or verify_data != data:
                raise MagicCliError("Verify failed: read-back data does not match source file!")
            print("Verification SUCCESS.")

        return save_type, host_crc

    finally:
        sess.ser.timeout = old_timeout
        sess.cfg.retries = old_retries
        sess.cfg.tx_inter_chunk_delay_s = old_delay


def n64_controller_probe(sess: MagicSession) -> tuple[int, int]:
    sess.send_cmd(CMD_N64_CONTROLLER_PROBE)
    data = sess.recv_reliable(3)
    cmd, st, dev_crc = sess.recv_rsp(CMD_N64_CONTROLLER_PROBE)
    require_ok(cmd, st, dev_crc)

    host_crc = crc16(data)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after controller probe: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )

    device = (data[0] << 8) | data[1]
    status = data[2]
    return device, status


def n64_controller_poll(sess: MagicSession, rumble: bool = False) -> tuple[int, int, int]:
    sess.send_cmd(CMD_N64_CONTROLLER_POLL, arg0=1 if rumble else 0)
    data = sess.recv_reliable(4)
    cmd, st, dev_crc = sess.recv_rsp(CMD_N64_CONTROLLER_POLL)
    require_ok(cmd, st, dev_crc)

    host_crc = crc16(data)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after controller poll: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )

    buttons = (data[0] << 8) | data[1]
    stick_x = data[2] - 256 if data[2] >= 128 else data[2]
    stick_y = data[3] - 256 if data[3] >= 128 else data[3]
    return buttons, stick_x, stick_y


def n64_controller_button_names(buttons: int) -> list[str]:
    hi = (buttons >> 8) & 0xFF
    lo = buttons & 0xFF
    names = []
    # Byte 0: A/B/Z/Start/D-pad bits.
    if hi & 0x80:
        names.append("A")
    if hi & 0x40:
        names.append("B")
    if hi & 0x20:
        names.append("Z")
    if hi & 0x10:
        names.append("Start")
    if hi & 0x08:
        names.append("D-Up")
    if hi & 0x04:
        names.append("D-Down")
    if hi & 0x02:
        names.append("D-Left")
    if hi & 0x01:
        names.append("D-Right")

    # Byte 1: shoulder and C-button bits.
    if lo & 0x20:
        names.append("L")
    if lo & 0x10:
        names.append("R")
    if lo & 0x08:
        names.append("C-Up")
    if lo & 0x04:
        names.append("C-Down")
    if lo & 0x02:
        names.append("C-Left")
    if lo & 0x01:
        names.append("C-Right")
    return names


def n64_export_mpk(sess: MagicSession, out_path: str) -> int:
    old_timeout = sess.ser.timeout
    old_retries = sess.cfg.retries
    sess.ser.timeout = max(float(old_timeout), 10.0)
    sess.cfg.retries = max(int(old_retries), 10)

    try:
        sess.send_cmd(CMD_N64_EXPORT_MPK)
        host_crc = sess.recv_reliable_to_file(N64_MEMPAK_SIZE, out_path)
        cmd, st, dev_crc = sess.recv_rsp(CMD_N64_EXPORT_MPK)
        require_ok(cmd, st, dev_crc)
        if host_crc != dev_crc:
            raise MagicCliError(
                f"CRC mismatch after MPK export: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
            )
        return host_crc
    finally:
        sess.ser.timeout = old_timeout
        sess.cfg.retries = old_retries


def n64_import_mpk(sess: MagicSession, in_path: str, verify: bool = False) -> int:
    if not os.path.exists(in_path):
        raise MagicCliError(f"Input file not found: {in_path}")
    with open(in_path, "rb") as f:
        data = f.read()
    if len(data) != N64_MEMPAK_SIZE:
        raise MagicCliError(
            f"MPK import requires exactly {N64_MEMPAK_SIZE} bytes, got {len(data)}"
        )

    old_timeout = sess.ser.timeout
    old_retries = sess.cfg.retries
    old_delay = sess.cfg.tx_inter_chunk_delay_s
    sess.ser.timeout = max(float(old_timeout), 10.0)
    sess.cfg.retries = max(int(old_retries), 10)
    # Give firmware time to commit each 512-byte chunk to mempak before next chunk.
    sess.cfg.tx_inter_chunk_delay_s = max(float(old_delay), 0.08)

    try:
        sess.send_cmd(CMD_N64_IMPORT_MPK)
        sess.send_reliable(data)
        cmd, st, dev_crc = sess.recv_rsp(CMD_N64_IMPORT_MPK)
        require_ok(cmd, st, dev_crc)

        host_crc = crc16(data)
        if host_crc != dev_crc:
            raise MagicCliError(
                f"CRC mismatch after MPK import: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
            )

        if verify:
            print(f"\nVerifying MPK import (reading back {N64_MEMPAK_SIZE} bytes)...")
            sess.send_cmd(CMD_N64_EXPORT_MPK)
            verify_data = sess.recv_reliable(N64_MEMPAK_SIZE)
            _cmd, _st, verify_crc = sess.recv_rsp(CMD_N64_EXPORT_MPK)
            require_ok(_cmd, _st, verify_crc)
            if verify_crc != host_crc or verify_data != data:
                 raise MagicCliError("Verify failed: MPK read-back data does not match source!")
            print("Verification SUCCESS.")

        return host_crc
    finally:
        sess.ser.timeout = old_timeout
        sess.cfg.retries = old_retries
        sess.cfg.tx_inter_chunk_delay_s = old_delay

def n64_gs_export(sess: MagicSession, out_path: str) -> tuple[int, int]:
    info = n64_gs_info(sess)
    if info["probe_status"] != ST_OK or not info["present"]:
        raise MagicCliError(
            "GameShark not detected or unsupported: "
            f"status={info['probe_status_name']} "
            f"flash_id=0x{info['flash_id']:04X} mfg_id=0x{info['mfg_id']:04X}"
        )

    gs_size = int(info["size_bytes"])
    if gs_size <= 0 or (gs_size & 1) != 0:
        raise MagicCliError(f"GameShark reported invalid size: {gs_size}")

    print(f"Dumping GameShark flash ({gs_size // 1024} KiB)...")
    sess.send_cmd(CMD_N64_GS_EXPORT)
    data = sess.recv_reliable(gs_size)
    cmd, st, dev_crc = sess.recv_rsp(CMD_N64_GS_EXPORT)
    require_ok(cmd, st, dev_crc)

    host_crc = crc16(data)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after GS export: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )

    with open(out_path, "wb") as f:
        f.write(data)

    return gs_size, host_crc

def n64_gs_import(sess: MagicSession, in_path: str) -> tuple[int, int]:
    info = n64_gs_info(sess)
    if info["probe_status"] != ST_OK or not info["present"]:
        raise MagicCliError(
            "GameShark not detected or unsupported: "
            f"status={info['probe_status_name']} "
            f"flash_id=0x{info['flash_id']:04X} mfg_id=0x{info['mfg_id']:04X}"
        )

    gs_size = int(info["size_bytes"])
    if gs_size <= 0 or (gs_size & 1) != 0:
        raise MagicCliError(f"GameShark reported invalid size: {gs_size}")

    file_size = os.path.getsize(in_path)
    if file_size > gs_size:
        raise MagicCliError(f"File size ({file_size} bytes) exceeds GameShark capacity ({gs_size} bytes)")
    if (file_size & 1) != 0:
        raise MagicCliError("File size must be a multiple of 2 (16-bit word aligned)")

    with open(in_path, "rb") as f:
        data = f.read()

    # Pad data with 0xFF to full capacity
    if len(data) < gs_size:
        data += b"\xFF" * (gs_size - len(data))

    print(f"Flashing GameShark ({gs_size // 1024} KiB)...")
    sess.send_cmd(CMD_N64_GS_IMPORT)
    sess.send_reliable(data)

    cmd, st, dev_crc = sess.recv_rsp(CMD_N64_GS_IMPORT)
    require_ok(cmd, st, dev_crc)

    host_crc = crc16(data)
    if host_crc != dev_crc:
        raise MagicCliError(
            f"CRC mismatch after GS import: pico=0x{dev_crc:04X}, host=0x{host_crc:04X}"
        )

    return gs_size, host_crc

def n64_gs_info(sess: MagicSession) -> dict:
    sess.send_cmd(CMD_N64_GS_INFO)
    data = sess.recv_reliable(18)
    cmd, st, _flash_id = sess.recv_rsp(CMD_N64_GS_INFO)

    if cmd != CMD_N64_GS_INFO:
        raise MagicCliError(
            f"Response cmd mismatch: got 0x{cmd:02X}, expected 0x{CMD_N64_GS_INFO:02X}"
        )

    # struct n64_gs_info_t has 18 bytes packed: u8, u8, u16, u32, u32, u16, u16, u16
    present, family, f_id, base_addr, size_bytes, caps, flags, mfg_id = struct.unpack("<BBHIIHHH", data)
    status_name = {
        0x00: "OK",
        0x01: "BAD_CMD",
        0x02: "IO_ERR",
        0x03: "BAD_ARG",
    }.get(st, f"UNKNOWN(0x{st:02X})")

    return {
        "probe_status": st,
        "probe_status_name": status_name,
        "present": bool(present),
        "family": family,
        "flash_id": f_id,
        "base_addr": base_addr,
        "size_bytes": size_bytes,
        "caps": caps,
        "flags": flags,
        "mfg_id": mfg_id
    }
