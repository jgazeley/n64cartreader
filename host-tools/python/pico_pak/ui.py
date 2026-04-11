from __future__ import annotations

from datetime import date
from pathlib import Path
import os

from .hexdump import hexdump_file, print_hexdump_legacy
from .integrity import host_git_tag
from .n64_ops import (
    pico_firmware_version,
    n64_controller_button_names,
    n64_export_mpk,
    n64_controller_poll,
    n64_controller_probe,
    n64_dump_rom,
    n64_export_header,
    n64_export_save_bytes,
    n64_export_save,
    n64_gs_export,
    n64_gs_import,
    n64_gs_info,
    n64_import_mpk,
    n64_import_save,
    n64_rescan,
    n64_rescan_and_configure,
    n64_rom_info,
    n64_status,
    save_type_name,
)
from .protocol import CMD_PING, N64_HEADER_SIZE, N64_ROM_BASE, N64_SRAM_BASE, MagicCliError, require_ok
from .transport import Cfg, MagicSession

BANNER_HEADER = (
    "**************************************",
    "    RP2040 Command Line Interface     ",
    "======================================",
    "Mode:      N64 Headless Host Bridge",
    "Host Fix:  Prompt rescan on I/H/S/D/O/P (default Yes)",
)

MENUS: dict[str, tuple[str, tuple[tuple[str, str, str], ...]]] = {
    "main": (
        "Main Menu",
        (
            ("N", "N64 Options", "submenu:n64"),
            ("U", "Host Utilities", "submenu:host"),
            ("X", "Exit CLI", "exit"),
        ),
    ),
    "n64": (
        "N64 Options",
        (
            ("I", "ID:   Cart Information", "n64_info"),
            ("H", "VIEW: Raw Header", "n64_header"),
            ("S", "VIEW: Save Data", "n64_save_view"),
            ("D", "DUMP: ROM to File", "n64_dump_rom"),
            ("O", "EXPORT: Save to File", "n64_export_save"),
            ("P", "IMPORT: Save from File", "n64_import_save"),
            ("G", "GS:   GameShark Options", "submenu:gs"),
            ("C", "TEST: Controller Poll", "n64_controller_poll"),
            ("K", "EXPORT: MPK to File", "n64_export_mpk"),
            ("J", "IMPORT: MPK from File", "n64_import_mpk"),
            ("R", "RESCAN: Cartridge", "n64_rescan"),
            ("X", "Back", "back"),
        ),
    ),
    "gs": (
        "GameShark Options",
        (
            ("I", "INFO: Probe GameShark", "gs_info"),
            ("O", "EXPORT: Flash to File", "gs_export"),
            ("P", "IMPORT: Flash from File", "gs_import"),
            ("X", "Back", "back"),
        ),
    ),
    "host": (
        "Host Utilities",
        (
            ("G", "PING: Probe Pico Link", "ping"),
            ("F", "FILE: Hexdump File", "hexdump_file"),
            ("P", "SET: Serial Port", "set_port"),
            ("T", "SET: Timeout", "set_timeout"),
            ("B", "SET: Baud", "set_baud"),
            ("X", "Back", "back"),
        ),
    ),
}


def _host_build_tag() -> str:
    override = os.getenv("PICO_PAK_HOST_TAG", "").strip()
    if override:
        return override

    return host_git_tag(Path(__file__).resolve().parents[2]) or "nogit"


def _firmware_build_tag(cfg: Cfg) -> str:
    try:
        with MagicSession(cfg) as sess:
            text = pico_firmware_version(sess)
        return text if text else "(empty)"
    except MagicCliError as exc:
        msg = str(exc)
        if "does not support cmd 0x03" in msg:
            return "legacy-fw (no version cmd)"
        return f"unavailable ({msg})"
    except OSError as exc:
        return f"unavailable ({exc})"


def _print_banner(cfg: Cfg) -> None:
    lines = (
        *BANNER_HEADER,
        f"BuildTag:  {_firmware_build_tag(cfg)}",
        f"HostTag:   {_host_build_tag()}",
        f"Built:     {date.today():%b %d %Y}",
    )
    for line in lines:
        print(line)
    print("")


def _render_menu(menu_id: str, cfg: Cfg) -> None:
    title, items = MENUS[menu_id]
    print(f"\n=== {title} ===")
    print(f"[port={cfg.port} baud={cfg.baud} timeout={cfg.timeout}s]")
    for key, desc, _action in items:
        print(f" {key}) {desc}")
    print("")


def _save_type_detail(save_type: int, save_size: int) -> str:
    if save_type == 1:
        return f"SRAM ({save_size} bytes)"
    if save_type == 2:
        return f"FlashRAM ({save_size} bytes)"
    if save_type == 3:
        return f"EEPROM 4Kbit ({save_size} bytes)"
    if save_type == 4:
        return f"EEPROM 16Kbit ({save_size} bytes)"
    if save_type == 0:
        return "None"
    if save_type == 5 and save_size == 0:
        return "Unknown (Joybus conflict: unplug controller for EEPROM probe)"
    return f"Unknown ({save_size} bytes)"


def _header_be32(header: bytes, offset: int) -> int:
    return int.from_bytes(header[offset:offset + 4], "big")


def _prompt_rescan_before_cart_op() -> bool:
    while True:
        raw = input("Rescan cartridge before operation? [Y/n]: ").strip().lower()
        if not raw:
            return True
        key = raw[:1]
        if key == "y":
            return True
        if key == "n":
            return False
        print("Please enter Y or N.")


def _menu_action_ping(cfg: Cfg) -> None:
    with MagicSession(cfg) as sess:
        sess.send_cmd(CMD_PING)
        cmd, st, val = sess.recv_rsp(CMD_PING)
        require_ok(cmd, st, val)
        print(f"PONG: 0x{val:04X}")


def _menu_action_n64_info(cfg: Cfg, rescan: bool = False) -> None:
    with MagicSession(cfg) as sess:
        # Rescan once (on header export) so all subsequent reads use fresh cart state.
        header, _ = n64_export_header(sess, rescan=rescan)
        rom_size, _ = n64_rom_info(sess, rescan=False)
        save_type, save_size, _ = n64_status(sess, rescan=False)

    if len(header) != N64_HEADER_SIZE:
        raise MagicCliError(f"Expected {N64_HEADER_SIZE} header bytes, got {len(header)}")

    title = header[0x20:0x34].decode("ascii", errors="replace").rstrip(" \x00")
    game_id = header[0x3B:0x3F].decode("ascii", errors="replace")
    version = header[0x3F]

    print("\n>> Cartridge Information:")
    print(f'  Title           : "{title}"')
    print(f"  Game ID         : {game_id}")
    print(f"  Version         : 0x{version:02X}")
    print(f"  Initial Settings: 0x{_header_be32(header, 0x00):08X}")
    print(f"  Clock Rate      : 0x{_header_be32(header, 0x04):08X}")
    print(f"  Entry Point PC  : 0x{_header_be32(header, 0x08):08X}")
    print(f"  Release Address : 0x{_header_be32(header, 0x0C):08X}")
    print(f"  CRC1            : 0x{_header_be32(header, 0x10):08X}")
    print(f"  CRC2            : 0x{_header_be32(header, 0x14):08X}")
    print(f"  ROM Size        : {rom_size} bytes ({rom_size // (1024 * 1024)} MiB)")
    print(f"  Save Type       : {_save_type_detail(save_type, save_size)}")


def _menu_action_n64_header(cfg: Cfg, rescan: bool = False) -> None:
    with MagicSession(cfg) as sess:
        data, host_crc = n64_export_header(sess, rescan=rescan)
    print("\n--- N64 ROM Header (64 Bytes) ---")
    print_hexdump_legacy(
        data,
        base_addr=N64_ROM_BASE,
        width=16,
        show_ascii=True,
        show_header=True,
    )
    print(f"\nCRC16=0x{host_crc:04X}")


def _menu_action_n64_save_view(cfg: Cfg, rescan: bool = False) -> None:
    with MagicSession(cfg) as sess:
        save_type, data, save_size, host_crc = n64_export_save_bytes(sess, rescan=rescan)

    preview = data[:512]
    base_addr = N64_SRAM_BASE if save_type in (1, 2) else 0
    print(f"\n--- N64 Save Data (first {len(preview)} bytes) ---")
    print(
        f"Type={save_type_name(save_type)} ({save_type}), "
        f"size={save_size} bytes"
    )
    print_hexdump_legacy(
        preview,
        base_addr=base_addr,
        width=16,
        show_ascii=True,
        show_header=True,
    )
    print(f"\nCRC16(full save)=0x{host_crc:04X}")


def _menu_action_n64_dump_rom(cfg: Cfg, rescan: bool = False) -> None:
    out = input("ROM output file path [dump.z64]: ").strip() or "dump.z64"
    with MagicSession(cfg) as sess:
        rom_size, host_crc = n64_dump_rom(sess, out, rescan=rescan)
    print(f"ROM dump complete -> {out} ({rom_size} bytes, crc16=0x{host_crc:04X})")


def _menu_action_n64_export_save(cfg: Cfg, rescan: bool = False) -> None:
    out = input("Save output file path [save.bin]: ").strip() or "save.bin"
    with MagicSession(cfg) as sess:
        save_type, data_len, _expected_size, host_crc = n64_export_save(sess, out, rescan=rescan)
    print(
        f"Save export complete -> {out} "
        f"({data_len} bytes, type={save_type_name(save_type)}, crc16=0x{host_crc:04X})"
    )


def _menu_action_n64_import_save(cfg: Cfg, rescan: bool = False) -> None:
    in_path = input("Save input file path: ").strip()
    if not in_path:
        raise MagicCliError("Input path required")
    with MagicSession(cfg) as sess:
        save_type, host_crc = n64_import_save(sess, in_path, rescan=rescan)
    print(
        f"Save import complete from {in_path} "
        f"(type={save_type_name(save_type)}, crc16=0x{host_crc:04X})"
    )


def _menu_action_gs_info(cfg: Cfg) -> None:
    with MagicSession(cfg) as sess:
        info = n64_gs_info(sess)

    size_bytes = int(info["size_bytes"])
    size_text = f"{size_bytes} bytes ({size_bytes // 1024} KiB)" if size_bytes > 0 else "0 bytes"
    print("\n>> GameShark Detection Info:")
    print(f"  Probe Status : {info['probe_status_name']} (0x{info['probe_status']:02X})")
    print(f"  Present      : {'Yes' if info['present'] else 'No'}")
    print(f"  Family       : {info['family']}")
    print(f"  Flash ID     : 0x{info['flash_id']:04X}")
    print(f"  Manufacturer : 0x{info['mfg_id']:04X}")
    print(f"  Base Address : 0x{info['base_addr']:08X}")
    print(f"  Capacity     : {size_text}")
    print(f"  Caps         : 0x{info['caps']:04X}")
    print(f"  Flags        : 0x{info['flags']:04X}")


def _menu_action_gs_export(cfg: Cfg) -> None:
    out = input("GameShark output file path [gameshark.bin]: ").strip() or "gameshark.bin"
    with MagicSession(cfg) as sess:
        gs_size, host_crc = n64_gs_export(sess, out)
    print(f"GameShark export complete -> {out} ({gs_size} bytes, crc16=0x{host_crc:04X})")


def _menu_action_gs_import(cfg: Cfg) -> None:
    in_path = input("GameShark input file path: ").strip()
    if not in_path:
        raise MagicCliError("Input path required")
    with MagicSession(cfg) as sess:
        gs_size, host_crc = n64_gs_import(sess, in_path)
    print(f"GameShark import complete from {in_path} ({gs_size} bytes, crc16=0x{host_crc:04X})")


def _menu_action_n64_rescan(cfg: Cfg) -> None:
    with MagicSession(cfg) as sess:
        result = n64_rescan_and_configure(sess)
    if result and result.get("is_gameshark"):
        print("N64 rescan complete — GameShark detected. Use GS commands for flash operations.")
    elif result and result.get("matched"):
        matched = result["matched"]
        print(f"N64 rescan complete — identified: {matched.get('title', '?')} ({matched.get('save_type_hint', '?')})")
    else:
        print("N64 rescan complete — cart not identified, save config not restored")


def _menu_action_n64_controller_poll(cfg: Cfg) -> None:
    with MagicSession(cfg) as sess:
        sess.cfg.quiet = True
        device, status = n64_controller_probe(sess)
        buttons, stick_x, stick_y = n64_controller_poll(sess, rumble=False)

    pressed = n64_controller_button_names(buttons)
    pressed_text = ", ".join(pressed) if pressed else "(none)"
    b0 = (buttons >> 8) & 0xFF
    b1 = buttons & 0xFF
    print("\n--- N64 Controller ---")
    print(f"Device=0x{device:04X}, status=0x{status:02X}")
    print(f"Buttons=0x{buttons:04X} (b0=0x{b0:02X}, b1=0x{b1:02X}) [{pressed_text}]")
    print(f"Stick=({stick_x},{stick_y})")


def _menu_action_n64_export_mpk(cfg: Cfg) -> None:
    out = input("MPK output file path [controller.mpk]: ").strip() or "controller.mpk"
    print("Exporting MPK... this may take a little while.")
    with MagicSession(cfg) as sess:
        sess.cfg.quiet = False
        host_crc = n64_export_mpk(sess, out)
    print(f"MPK export complete -> {out} (32768 bytes, crc16=0x{host_crc:04X})")


def _menu_action_n64_import_mpk(cfg: Cfg) -> None:
    in_path = input("MPK input file path: ").strip()
    if not in_path:
        raise MagicCliError("Input path required")
    print("Importing MPK... this may take a little while.")
    with MagicSession(cfg) as sess:
        sess.cfg.quiet = False
        host_crc = n64_import_mpk(sess, in_path)
    print(f"MPK import complete from {in_path} (crc16=0x{host_crc:04X})")


def _menu_action_hexdump_file() -> None:
    path = input("File path: ").strip()
    if not path:
        raise MagicCliError("File path required")
    off = input("Offset (dec/hex, default 0): ").strip() or "0"
    ln = input("Length (dec/hex, default 256): ").strip() or "256"
    hexdump_file(path, off, ln)


def _menu_action_set_port(cfg: Cfg) -> None:
    new_port = input(f"Port [{cfg.port}]: ").strip()
    if new_port:
        cfg.port = new_port


def _menu_action_set_timeout(cfg: Cfg) -> None:
    raw = input(f"Timeout seconds [{cfg.timeout}]: ").strip()
    if raw:
        cfg.timeout = float(raw)


def _menu_action_set_baud(cfg: Cfg) -> None:
    raw = input(f"Baud [{cfg.baud}]: ").strip()
    if raw:
        cfg.baud = int(raw, 10)


def run_interactive(cfg: Cfg) -> int:
    _print_banner(cfg)
    print("Interactive mode. Press Ctrl+C or choose X to quit.")

    def _run_with_optional_rescan(action_fn):
        return action_fn(rescan=_prompt_rescan_before_cart_op())

    action_map = {
        "ping": lambda: _menu_action_ping(cfg),
        "n64_info": lambda: _run_with_optional_rescan(lambda rescan: _menu_action_n64_info(cfg, rescan=rescan)),
        "n64_header": lambda: _run_with_optional_rescan(lambda rescan: _menu_action_n64_header(cfg, rescan=rescan)),
        "n64_save_view": lambda: _run_with_optional_rescan(lambda rescan: _menu_action_n64_save_view(cfg, rescan=rescan)),
        "n64_dump_rom": lambda: _run_with_optional_rescan(lambda rescan: _menu_action_n64_dump_rom(cfg, rescan=rescan)),
        "n64_export_save": lambda: _run_with_optional_rescan(lambda rescan: _menu_action_n64_export_save(cfg, rescan=rescan)),
        "n64_import_save": lambda: _run_with_optional_rescan(lambda rescan: _menu_action_n64_import_save(cfg, rescan=rescan)),
        "gs_info": lambda: _menu_action_gs_info(cfg),
        "gs_export": lambda: _menu_action_gs_export(cfg),
        "gs_import": lambda: _menu_action_gs_import(cfg),
        "n64_controller_poll": lambda: _menu_action_n64_controller_poll(cfg),
        "n64_export_mpk": lambda: _menu_action_n64_export_mpk(cfg),
        "n64_import_mpk": lambda: _menu_action_n64_import_mpk(cfg),
        "n64_rescan": lambda: _menu_action_n64_rescan(cfg),
        "hexdump_file": _menu_action_hexdump_file,
        "set_port": lambda: _menu_action_set_port(cfg),
        "set_timeout": lambda: _menu_action_set_timeout(cfg),
        "set_baud": lambda: _menu_action_set_baud(cfg),
    }

    menu_stack = ["main"]
    while True:
        menu_id = menu_stack[-1]
        _, items = MENUS[menu_id]
        _render_menu(menu_id, cfg)

        try:
            raw = input("Select: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("")
            return 0

        key = raw[:1].upper()
        item = None
        for candidate in items:
            if candidate[0] == key:
                item = candidate
                break

        if item is None:
            shown = raw[:1] if raw else ""
            print(f"Unknown option: '{shown}'")
            continue

        action = item[2]
        if action == "exit":
            return 0
        if action == "back":
            if len(menu_stack) > 1:
                menu_stack.pop()
            continue
        if action.startswith("submenu:"):
            menu_stack.append(action.split(":", 1)[1])
            continue

        try:
            fn = action_map.get(action)
            if fn is None:
                raise MagicCliError(f"Menu action not implemented: {action}")
            fn()
        except (MagicCliError, OSError, ValueError) as exc:
            print(f"[ERROR] {exc}")
