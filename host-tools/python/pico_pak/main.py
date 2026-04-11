from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys
import tempfile
import time

from .cart_id import crc32_ieee_file, identify_cart, save_hint_to_fw_config
from .hexdump import hexdump_file, print_hexdump_legacy
from .integrity import (
    build_artifact_manifest,
    normalize_sha256_hex,
    sha256_file,
    write_manifest,
)
from .n64_ops import (
    pico_firmware_version,
    n64_controller_button_names,
    n64_export_mpk,
    n64_controller_poll,
    n64_controller_probe,
    n64_dump_rom,
    n64_export_header,
    n64_export_sram_raw,
    n64_export_save,
    n64_gs_info,
    n64_gs_export,
    n64_gs_import,
    n64_import_mpk,
    n64_import_save,
    n64_mx29lv640_export_window_stream,
    n64_rescan,
    n64_rescan_and_configure,
    n64_repro_read_word,
    n64_repro_rom_diagnostic,
    n64_rom_flash_erase,
    n64_rom_flash_id,
    n64_rom_info,
    n64_set_rom_size,
    n64_set_save_cfg,
    n64_status,
    save_type_name,
)
from .protocol import (
    CMD_CLEARBUF,
    CMD_CRC16,
    CMD_EXPORTBUF,
    CMD_FILLBUF,
    CMD_GET_INFO,
    CMD_IMPORT32K,
    CMD_IMPORTBUF,
    CMD_PING,
    CMD_STREAM32K,
    CMD_SWAP16,
    CMD_SWAP32,
    CMD_WRITEMSG,
    DEFAULT_BAUD,
    DEFAULT_CHUNK,
    DEFAULT_PORT,
    DEFAULT_RETRIES,
    DEFAULT_TIMEOUT,
    MagicCliError,
    N64_ROM_BASE,
    crc16,
    require_ok,
)
from .transport import Cfg, MagicSession
from .ui import run_interactive


def _add_integrity_args(p: argparse.ArgumentParser) -> None:
    p.add_argument(
        "--sha256",
        action="store_true",
        help="Compute and print artifact SHA256",
    )
    p.add_argument(
        "--verify-sha256",
        help="Expected SHA256 hex; fail if artifact hash does not match",
    )
    p.add_argument(
        "--manifest-out",
        help="Write JSON integrity manifest to this path",
    )


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="pico_pak_n64_magic_cli",
        description="Host CLI for headless raw-magic Pico firmware (interactive or subcommands)",
    )
    p.add_argument("--port", default=DEFAULT_PORT)
    p.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    p.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT)
    p.add_argument("--chunk-size", type=int, default=DEFAULT_CHUNK)
    p.add_argument("--retries", type=int, default=DEFAULT_RETRIES)
    p.add_argument("--quiet", action="store_true")

    sub = p.add_subparsers(dest="sub", required=False)
    sub.add_parser("interactive", help="Launch simple interactive menu UI")
    sub.add_parser("ping")
    sub.add_parser("info")
    sub.add_parser("fw-version")

    p_fill = sub.add_parser("fillbuf")
    p_fill.add_argument("--seed", type=int, default=0)
    sub.add_parser("clearbuf")
    sub.add_parser("writemsg")
    sub.add_parser("crc16")

    p_s16 = sub.add_parser("swap16")
    p_s16.add_argument("--nwords", type=int, default=0)

    p_s32 = sub.add_parser("swap32")
    p_s32.add_argument("--nwords", type=int, default=0)

    p_exp = sub.add_parser("exportbuf")
    p_exp.add_argument("--out", required=True)

    p_imp = sub.add_parser("importbuf")
    p_imp.add_argument("--in", dest="input_file", required=True)

    p_stx = sub.add_parser("stream32k")
    p_stx.add_argument("--out", required=True)

    p_srx = sub.add_parser("import32k")
    p_srx.add_argument("--in", dest="input_file", required=True)

    sub.add_parser("n64-rescan")
    sub.add_parser(
        "n64-repro-flash-id",
        aliases=["n64-rom-flash-id"],
        help="Experimental repro NOR probe (legacy alias: n64-rom-flash-id)",
    )
    sub.add_parser(
        "n64-repro-flash-erase",
        aliases=["n64-rom-flash-erase"],
        help="Experimental repro NOR chip erase (legacy alias: n64-rom-flash-erase)",
    )
    sub.add_parser(
        "n64-repro-rom-diagnostic",
        aliases=["n64-rom-flash-write-test"],
        help="Experimental non-destructive repro ROM aliasing diagnostic (legacy alias: n64-rom-flash-write-test)",
    )
    p_rrw = sub.add_parser(
        "n64-repro-read-word",
        help="Experimental repro ROM word read at an arbitrary byte offset",
    )
    p_rrw.add_argument(
        "offset",
        type=lambda value: int(value, 0),
        help="Even byte offset from ROM base (accepts decimal or 0x-prefixed hex)",
    )
    p_rrw.add_argument(
        "--reset-carrier",
        action="store_true",
        help="Send the repro flash-id reset carrier before reading the word",
    )
    p_mx29 = sub.add_parser(
        "n64-mx29-export-window-stream",
        help="Experimental 8MB MX29LV640 raw stream read of one 512-byte ROM window",
    )
    p_mx29.add_argument(
        "offset",
        type=lambda value: int(value, 0),
        help="Even byte offset from ROM base (accepts decimal or 0x-prefixed hex)",
    )

    p_ns = sub.add_parser("n64-status")
    p_ns.add_argument("--rescan", action="store_true")

    p_nss = sub.add_parser("n64-set-save", help="Set save type on firmware (host-directed)")
    p_nss.add_argument(
        "save_type",
        choices=["none", "sram", "sram96k", "flashram", "eeprom4k", "eeprom16k"],
        help="Save type to configure on firmware",
    )

    p_nri = sub.add_parser("n64-rom-info")
    p_nri.add_argument("--rescan", action="store_true")

    p_ncid = sub.add_parser("n64-cart-id")
    p_ncid.add_argument("--rescan", action="store_true")
    p_ncid.add_argument("--db", help="Optional cart ID database JSON override (not used by default)")
    p_ncid.add_argument(
        "--verify-rom-crc32",
        action="store_true",
        help="Compute full ROM CRC32/IEEE and include it in match scoring",
    )
    p_ncid.add_argument(
        "--rom-file",
        help="Use an existing ROM dump file as CRC32 source (requires --verify-rom-crc32)",
    )
    p_ncid.add_argument(
        "--keep-temp-rom",
        action="store_true",
        help="Keep temporary ROM dump file created by --verify-rom-crc32",
    )
    p_ncid.add_argument(
        "--catalog-txt",
        help="Path to N64 text catalog (defaults to host-tools/n64.txt)",
    )
    p_ncid.add_argument(
        "--no-catalog-txt",
        action="store_true",
        help="Disable text-catalog matching and only use JSON DB entries",
    )
    p_ncid.add_argument("--json", action="store_true", dest="json_output")
    p_ncid.add_argument(
        "--emit-unknown",
        help="Append unknown cart signatures to a JSONL file for later curation",
    )

    p_nh = sub.add_parser("n64-header")
    p_nh.add_argument("--rescan", action="store_true")
    p_nh.add_argument("--out", help="Optional output file for the raw 64-byte header")

    p_nrd = sub.add_parser("n64-rom-dump")
    p_nrd.add_argument("--out", required=True)
    p_nrd.add_argument("--rescan", action="store_true")
    _add_integrity_args(p_nrd)

    p_nexp = sub.add_parser("n64-export")
    p_nexp.add_argument("--out", required=True)
    p_nexp.add_argument("--rescan", action="store_true")
    _add_integrity_args(p_nexp)

    p_nraw = sub.add_parser("n64-sram-raw-export")
    p_nraw.add_argument("--out", required=True)
    p_nraw.add_argument("--size", type=int, default=32768, help="Bytes to read from SRAM base (default 32768)")
    p_nraw.add_argument("--rescan", action="store_true")
    _add_integrity_args(p_nraw)

    p_nimp = sub.add_parser("n64-import")
    p_nimp.add_argument("--in", dest="input_file", required=True)
    p_nimp.add_argument("--rescan", action="store_true")
    p_nimp.add_argument("--verify", action="store_true")
    _add_integrity_args(p_nimp)

    p_gse = sub.add_parser("n64-gs-export")
    p_gse.add_argument("--out", required=True)
    _add_integrity_args(p_gse)

    p_gsi = sub.add_parser("n64-gs-import")
    p_gsi.add_argument("--in", dest="input_file", required=True)

    sub.add_parser("n64-gs-info")

    sub.add_parser("n64-controller-probe")

    p_ncp = sub.add_parser("n64-controller-poll")
    p_ncp.add_argument("--rumble", action="store_true")

    p_nct = sub.add_parser("n64-controller-test")
    p_nct.add_argument("--hz", type=int, default=30)
    p_nct.add_argument("--rumble", action="store_true")

    p_nme = sub.add_parser("n64-mpk-export")
    p_nme.add_argument("--out", required=True)
    _add_integrity_args(p_nme)

    p_nmi = sub.add_parser("n64-mpk-import")
    p_nmi.add_argument("--in", dest="input_file", required=True)
    p_nmi.add_argument("--verify", action="store_true")
    _add_integrity_args(p_nmi)

    p_hf = sub.add_parser("hexdump-file")
    p_hf.add_argument("--in", dest="input_file", required=True)
    p_hf.add_argument("--offset", default="0")
    p_hf.add_argument("--length", default="256")

    return p


def _print_cart_id_result(result: dict) -> None:
    fingerprint = result["fingerprint"]
    database = result["database"]
    catalog = result["catalog"]

    if result.get("is_gameshark"):
        print("N64 Cart ID:")
        print("  Match level : gameshark")
        print(f"  Title       : {fingerprint['title']}")
        print(f"  CRC1/CRC2   : {fingerprint['crc1']} / {fingerprint['crc2']}")
        print("")
        print("  ** GameShark / Action Replay detected **")
        print("  Save export/import does not apply to this device.")
        print("  Use n64-gs-info, n64-gs-export, n64-gs-import instead.")
        return

    print("N64 Cart ID:")
    print(f"  Match level : {result['match_level']}")
    print(f"  DB path     : {database['path']}")
    print(f"  DB entries  : {database['entry_count']} (schema v{database['schema_version']})")
    if catalog["path"]:
        print(f"  Catalog txt : {catalog['path']}")
        print(f"  Catalog n   : {catalog['entry_count']}")
    else:
        print("  Catalog txt : (disabled/not found)")
    print(f"  Title       : {fingerprint['title']}")
    print(f"  Game ID     : {fingerprint['game_id']} (hex {fingerprint['game_id_hex']})")
    print(f"  Region      : {fingerprint['region_name']} ({fingerprint['region_code']})")
    print(f"  Version     : 0x{fingerprint['version']:02X}")
    print(f"  ROM size    : {fingerprint['rom_size']} bytes")
    print(f"  CRC1/CRC2   : {fingerprint['crc1']} / {fingerprint['crc2']}")
    if fingerprint.get("rom_crc32_ieee"):
        print(f"  ROM CRC32   : {fingerprint['rom_crc32_ieee']}")
    if result.get("rom_crc32_source"):
        print(f"  CRC32 src   : {result['rom_crc32_source']}")

    observed_save_type = result.get("observed_save_type")
    if observed_save_type:
        print(f"  Save type   : {observed_save_type}")

    matched = result.get("matched")
    if matched is not None:
        print("")
        print("Best match:")
        print(f"  Slug        : {matched.get('slug', '(none)')}")
        print(f"  Title       : {matched.get('title', '(none)')}")
        print(f"  Game ID     : {matched.get('game_id', '(none)')}")
        print(f"  Region      : {matched.get('region', '(none)')}")
        print(f"  Source      : {matched.get('source', '(none)')}")
        if matched.get("catalog_line"):
            print(f"  Catalog line: {matched.get('catalog_line')}")
        if matched.get("save_type_hint"):
            print(f"  Save hint   : {matched.get('save_type_hint')}")
        print(f"  Reason      : {matched.get('reason', '(none)')}")

    candidates = result.get("candidates", [])
    if candidates:
        print("")
        print("Top candidates:")
        for idx, candidate in enumerate(candidates[:3], start=1):
            print(
                f"  {idx}. {candidate.get('slug', '(none)')} "
                f"[{candidate.get('match_level', 'unknown')}] - "
                f"{candidate.get('reason', '(none)')} "
                f"(source={candidate.get('source', 'db')})"
            )

    warnings = result.get("warnings", [])
    if warnings:
        print("")
        print("Warnings:")
        for warning in warnings:
            print(f"  - {warning.get('message', '(unknown warning)')}")

    rom_cfg = result.get("rom_size_configured")
    save_cfg = result.get("save_configured")
    if rom_cfg is not None or save_cfg is not None:
        print("")
        print("Firmware configured:")
        if rom_cfg is not None:
            print(f"  ROM size  : {rom_cfg} bytes ({rom_cfg // (1024 * 1024)} MiB)")
        if save_cfg is not None:
            print(f"  Save type : {save_cfg['type']} ({save_cfg['size_bytes']} bytes)")

    unknown_log = result.get("unknown_logged_to")
    if unknown_log:
        print(f"\nUnknown signature appended to: {unknown_log}")


def _integrity_requested(args: argparse.Namespace) -> bool:
    return bool(
        getattr(args, "sha256", False)
        or getattr(args, "verify_sha256", None)
        or getattr(args, "manifest_out", None)
    )


def _resolve_artifact_integrity(
    args: argparse.Namespace,
    artifact_path: str,
) -> tuple[str, int, str | None]:
    abs_path = str(Path(artifact_path).expanduser().resolve())
    sha256_hex = sha256_file(abs_path).lower()
    size_bytes = os.path.getsize(abs_path)

    expected_hash = None
    raw_expected = getattr(args, "verify_sha256", None)
    if raw_expected:
        expected_hash = normalize_sha256_hex(raw_expected)
        if sha256_hex != expected_hash:
            raise MagicCliError(
                f"SHA256 mismatch: got {sha256_hex}, expected {expected_hash}"
            )
        print(f"SHA256 verify OK: {sha256_hex}")

    if getattr(args, "sha256", False):
        print(f"SHA256: {sha256_hex}")

    return sha256_hex, size_bytes, expected_hash


def _get_firmware_for_manifest(sess: MagicSession) -> str | None:
    try:
        return pico_firmware_version(sess)
    except Exception:
        # Manifest should still be useful even if a version query fails.
        return None


def _maybe_write_integrity_manifest(
    sess: MagicSession,
    args: argparse.Namespace,
    *,
    operation: str,
    direction: str,
    artifact_path: str,
    sha256_hex: str,
    size_bytes: int,
    crc16_u16: int | None = None,
    metadata: dict | None = None,
) -> None:
    manifest_out = getattr(args, "manifest_out", None)
    if not manifest_out:
        return

    manifest = build_artifact_manifest(
        operation=operation,
        direction=direction,
        artifact_path=artifact_path,
        artifact_size=size_bytes,
        sha256_hex=sha256_hex,
        crc16_u16=crc16_u16,
        firmware_version=_get_firmware_for_manifest(sess),
        serial_port=getattr(args, "port", None),
        metadata=metadata,
    )
    manifest_path = write_manifest(manifest_out, manifest)
    print(f"Wrote manifest: {manifest_path}")


def main() -> int:
    args = parser().parse_args()
    cfg = Cfg(
        port=args.port,
        baud=args.baud,
        timeout=args.timeout,
        chunk_size=args.chunk_size,
        retries=args.retries,
        quiet=args.quiet,
    )

    try:
        if args.sub in (None, "interactive"):
            return run_interactive(cfg)

        if args.sub == "hexdump-file":
            hexdump_file(args.input_file, args.offset, args.length)
            return 0

        with MagicSession(cfg) as sess:
            if args.sub == "ping":
                sess.send_cmd(CMD_PING)
                cmd, st, val = sess.recv_rsp(CMD_PING)
                require_ok(cmd, st, val)
                print(f"PONG: 0x{val:04X}")

            elif args.sub == "info":
                sess.send_cmd(CMD_GET_INFO)
                cmd, st, val = sess.recv_rsp(CMD_GET_INFO)
                size = require_ok(cmd, st, val)
                print(f"Buffer size: {size} bytes")

            elif args.sub == "fw-version":
                print(pico_firmware_version(sess))

            elif args.sub == "fillbuf":
                sess.send_cmd(CMD_FILLBUF, arg0=args.seed & 0xFF)
                cmd, st, val = sess.recv_rsp(CMD_FILLBUF)
                require_ok(cmd, st, val)
                print(f"Filled buffer ({val} bytes), seed={args.seed & 0xFF}")

            elif args.sub == "clearbuf":
                sess.send_cmd(CMD_CLEARBUF)
                cmd, st, val = sess.recv_rsp(CMD_CLEARBUF)
                require_ok(cmd, st, val)
                print("Buffer cleared")

            elif args.sub == "writemsg":
                sess.send_cmd(CMD_WRITEMSG)
                cmd, st, val = sess.recv_rsp(CMD_WRITEMSG)
                require_ok(cmd, st, val)
                print(f"Wrote message ({val} bytes)")

            elif args.sub == "crc16":
                sess.send_cmd(CMD_CRC16)
                cmd, st, val = sess.recv_rsp(CMD_CRC16)
                require_ok(cmd, st, val)
                print(f"CRC16: 0x{val:04X}")

            elif args.sub == "swap16":
                sess.send_cmd(CMD_SWAP16, arg1=args.nwords & 0xFFFF)
                cmd, st, val = sess.recv_rsp(CMD_SWAP16)
                require_ok(cmd, st, val)
                print(f"Swapped 16-bit words: {val}")

            elif args.sub == "swap32":
                sess.send_cmd(CMD_SWAP32, arg1=args.nwords & 0xFFFF)
                cmd, st, val = sess.recv_rsp(CMD_SWAP32)
                require_ok(cmd, st, val)
                print(f"Swapped 32-bit words: {val}")

            elif args.sub == "exportbuf":
                sess.send_cmd(CMD_EXPORTBUF)
                data = sess.recv_reliable(512)
                cmd, st, val = sess.recv_rsp(CMD_EXPORTBUF)
                require_ok(cmd, st, val)
                with open(args.out, "wb") as f:
                    f.write(data)
                print(f"Exported {len(data)} bytes -> {args.out}")

            elif args.sub == "importbuf":
                if not os.path.exists(args.input_file):
                    raise MagicCliError(f"Input file not found: {args.input_file}")
                with open(args.input_file, "rb") as f:
                    data = f.read()
                if len(data) != 512:
                    raise MagicCliError(f"importbuf requires exactly 512 bytes, got {len(data)}")
                sess.send_cmd(CMD_IMPORTBUF)
                sess.send_reliable(data)
                cmd, st, val = sess.recv_rsp(CMD_IMPORTBUF)
                require_ok(cmd, st, val)
                print(f"Imported 512 bytes, resulting CRC16=0x{val:04X}")

            elif args.sub == "stream32k":
                sess.send_cmd(CMD_STREAM32K)
                data = sess.recv_reliable(32768)
                cmd, st, val = sess.recv_rsp(CMD_STREAM32K)
                require_ok(cmd, st, val)
                with open(args.out, "wb") as f:
                    f.write(data)
                print(f"Streamed {len(data)} bytes -> {args.out}")

            elif args.sub == "import32k":
                if not os.path.exists(args.input_file):
                    raise MagicCliError(f"Input file not found: {args.input_file}")
                with open(args.input_file, "rb") as f:
                    data = f.read()
                if len(data) != 32768:
                    raise MagicCliError(f"import32k requires exactly 32768 bytes, got {len(data)}")
                sess.send_cmd(CMD_IMPORT32K)
                sess.send_reliable(data)
                cmd, st, val = sess.recv_rsp(CMD_IMPORT32K)
                require_ok(cmd, st, val)
                print(f"Imported 32768 bytes, running CRC16=0x{val:04X}")

            elif args.sub == "n64-rescan":
                result = n64_rescan_and_configure(sess)
                if result and result.get("is_gameshark"):
                    print("N64 rescan complete — GameShark detected. Use GS commands for flash operations.")
                elif result and result.get("matched"):
                    matched = result["matched"]
                    print(f"N64 rescan complete — identified: {matched.get('title', '?')} ({matched.get('save_type_hint', '?')})")
                else:
                    print("N64 rescan complete — cart not identified, save config not restored")

            elif args.sub in ("n64-repro-flash-id", "n64-rom-flash-id"):
                info = n64_rom_flash_id(sess)
                mfg   = info["mfg_id"]
                dev0  = info["dev_id0"]
                dev1  = info["dev_id1"]
                dev2  = info["dev_id2"]
                MFG_NAMES = {
                    0x0020: "ST/Numonyx/Micron",
                    0x00C2: "Macronix",
                    0x0001: "AMD/Spansion",
                    0x0004: "Fujitsu",
                    0x00AD: "Hynix",
                    0x0089: "Intel",
                }
                mfg_name = MFG_NAMES.get(mfg, "Unknown")
                if info["present"]:
                    print("Experimental repro NOR probe result:")
                    print(f"  Manufacturer : 0x{mfg:04X} ({mfg_name})")
                    print(f"  Device ID 0  : 0x{dev0:04X}")
                    print(f"  Device ID 1  : 0x{dev1:04X}")
                    print(f"  Device ID 2  : 0x{dev2:04X}")
                else:
                    print(f"Experimental repro NOR probe: no flash chip detected (mfg_id=0x{mfg:04X}) — mask ROM, open bus, or unsupported repro.")

            elif args.sub in ("n64-repro-flash-erase", "n64-rom-flash-erase"):
                print("Running experimental repro NOR chip erase (this may take several minutes)...")
                result = n64_rom_flash_erase(sess)
                print(f"  Pre-erase ROM[0] : 0x{result['pre_word']:04X}")
                print(f"  First status reg : 0x{result['first_sr']:04X}")
                print(f"  Final status reg : 0x{result['final_sr']:04X}")
                print(f"  Post-erase ROM[0]: 0x{result['post_word']:04X}")
                print(f"  Poll iterations  : {result['poll_count']}")
                if result["success"]:
                    print("Experimental repro NOR chip erase complete.")
                else:
                    print("Experimental repro NOR chip erase FAILED (timeout or error).")

            elif args.sub in ("n64-repro-rom-diagnostic", "n64-rom-flash-write-test"):
                print("Running experimental non-destructive repro ROM diagnostic...")
                result = n64_repro_rom_diagnostic(sess)
                for offset in result["offsets"]:
                    print(f"  0x{offset:06X} : 0x{result['samples'][offset]:04X}")
                if not result["success"]:
                    print("Diagnostic command failed.")

            elif args.sub == "n64-repro-read-word":
                word = n64_repro_read_word(
                    sess,
                    args.offset,
                    reset_carrier=args.reset_carrier,
                )
                print(f"Repro ROM word at 0x{args.offset:06X}: 0x{word:04X}")

            elif args.sub == "n64-mx29-export-window-stream":
                data = n64_mx29lv640_export_window_stream(sess, args.offset)
                print(f"MX29LV640 streamed ROM window at 0x{args.offset:06X}:")
                print_hexdump_legacy(data)

            elif args.sub == "n64-status":
                save_type, save_size, meta = n64_status(sess, rescan=args.rescan)
                print(
                    f"N64 save status: type={save_type_name(save_type)} ({save_type}), "
                    f"size={save_size} bytes, meta=0x{meta:04X}"
                )

            elif args.sub == "n64-set-save":
                config = save_hint_to_fw_config(args.save_type)
                if config is None:
                    raise MagicCliError(f"Unknown save type: {args.save_type}")
                fw_type, fw_size = config
                confirmed_type, confirmed_size = n64_set_save_cfg(sess, fw_type, fw_size)
                print(
                    f"Save configured: type={save_type_name(confirmed_type)} ({confirmed_type}), "
                    f"size={confirmed_size} bytes"
                )

            elif args.sub == "n64-rom-info":
                rom_size, units2k = n64_rom_info(sess, rescan=args.rescan)
                print(
                    f"N64 ROM size: {rom_size} bytes "
                    f"({rom_size // (1024 * 1024)} MiB), units2k={units2k}"
                )

            elif args.sub == "n64-cart-id":
                if args.rom_file and not args.verify_rom_crc32:
                    raise MagicCliError("--rom-file requires --verify-rom-crc32")

                header, _ = n64_export_header(sess, rescan=args.rescan)
                try:
                    rom_size, _ = n64_rom_info(sess, rescan=False)
                except MagicCliError:
                    rom_size = 0
                save_type, _save_size, _meta = n64_status(sess, rescan=False)

                rom_crc32_u32 = None
                rom_crc32_source = None
                if args.verify_rom_crc32:
                    if args.rom_file:
                        if not os.path.exists(args.rom_file):
                            raise MagicCliError(f"ROM file not found: {args.rom_file}")
                        rom_crc32_u32 = crc32_ieee_file(args.rom_file)
                        rom_crc32_source = os.path.abspath(args.rom_file)
                    else:
                        if rom_size <= 0:
                            # Need ROM size before we can dump. Fall back to a
                            # preliminary identification only if firmware size
                            # detection is unavailable.
                            pre_result = identify_cart(
                                header,
                                0,
                                db_path=args.db,
                                catalog_txt_path=args.catalog_txt,
                                use_catalog_txt=not args.no_catalog_txt,
                            )
                            pre_match = pre_result.get("matched")
                            pre_rom_size = int(pre_match["rom_size"]) if pre_match and pre_match.get("rom_size") else 0
                            if pre_rom_size <= 0:
                                raise MagicCliError(
                                    "Cannot determine ROM size for CRC32 dump. "
                                    "Use --rom-file with an existing dump, or ensure cart is in n64.txt."
                                )
                            n64_set_rom_size(sess, pre_rom_size)
                            rom_size = pre_rom_size

                        tmp = tempfile.NamedTemporaryFile(
                            prefix="n64_cart_id_crc32_",
                            suffix=".z64",
                            delete=False,
                        )
                        tmp_path = tmp.name
                        tmp.close()

                        _dump_size, _dump_crc16 = n64_dump_rom(sess, tmp_path, rescan=False)
                        rom_crc32_u32 = crc32_ieee_file(tmp_path)
                        if args.keep_temp_rom:
                            rom_crc32_source = os.path.abspath(tmp_path)
                        else:
                            os.unlink(tmp_path)
                            rom_crc32_source = "(temporary dump removed)"

                result = identify_cart(
                    header,
                    rom_size,
                    db_path=args.db,
                    catalog_txt_path=args.catalog_txt,
                    use_catalog_txt=not args.no_catalog_txt,
                    rom_crc32_u32=rom_crc32_u32,
                    observed_save_type=save_type_name(save_type),
                    emit_unknown_path=args.emit_unknown,
                )
                if rom_crc32_source is not None:
                    result["rom_crc32_source"] = rom_crc32_source

                # Auto-configure ROM size and save type from match.
                # Skip for GameShark — save operations don't apply.
                matched = result.get("matched")
                if matched and not result.get("is_gameshark"):
                    # Only push ROM size if firmware detection did not supply it.
                    matched_rom_size = int(matched.get("rom_size") or 0)
                    if rom_size <= 0 and matched_rom_size > 0:
                        try:
                            confirmed = n64_set_rom_size(sess, matched_rom_size)
                            result["rom_size_configured"] = confirmed
                        except MagicCliError:
                            result["rom_size_configured"] = None
                    else:
                        result["rom_size_configured"] = rom_size if rom_size > 0 else None

                    # Push save type to firmware.
                    if matched.get("save_type_hint"):
                        cfg = save_hint_to_fw_config(matched["save_type_hint"])
                        if cfg is not None:
                            fw_type, fw_size = cfg
                            try:
                                ct, cs = n64_set_save_cfg(sess, fw_type, fw_size)
                                result["save_configured"] = {
                                    "type": save_type_name(ct),
                                    "type_id": ct,
                                    "size_bytes": cs,
                                }
                            except MagicCliError:
                                result["save_configured"] = None

                if args.json_output:
                    print(json.dumps(result, indent=2, sort_keys=True))
                else:
                    _print_cart_id_result(result)

            elif args.sub == "n64-rom-dump":
                rom_size, host_crc = n64_dump_rom(sess, args.out, rescan=args.rescan)
                print(
                    f"Dumped N64 ROM ({rom_size} bytes) -> {args.out}, "
                    f"CRC16=0x{host_crc:04X}"
                )
                if _integrity_requested(args):
                    sha256_hex, size_bytes, expected_hash = _resolve_artifact_integrity(args, args.out)
                    metadata = {
                        "rescan": bool(args.rescan),
                        "rom_size_bytes": rom_size,
                    }
                    if expected_hash is not None:
                        metadata["expected_sha256"] = expected_hash
                    _maybe_write_integrity_manifest(
                        sess,
                        args,
                        operation="n64-rom-dump",
                        direction="export",
                        artifact_path=args.out,
                        sha256_hex=sha256_hex,
                        size_bytes=size_bytes,
                        crc16_u16=host_crc,
                        metadata=metadata,
                    )

            elif args.sub == "n64-header":
                data, host_crc = n64_export_header(sess, rescan=args.rescan)
                if args.out:
                    with open(args.out, "wb") as f:
                        f.write(data)
                    print(f"Saved 64-byte header -> {args.out}")

                print("")
                print("--- N64 ROM Header (64 Bytes) ---")
                print_hexdump_legacy(
                    data,
                    base_addr=N64_ROM_BASE,
                    width=16,
                    show_ascii=True,
                    show_header=True,
                )
                print(f"\nCRC16: 0x{host_crc:04X}")

            elif args.sub == "n64-export":
                save_type, data_len, _save_size, host_crc = n64_export_save(sess, args.out, rescan=args.rescan)
                print(
                    f"Exported N64 save ({save_type_name(save_type)}, {data_len} bytes) -> {args.out}, "
                    f"CRC16=0x{host_crc:04X}"
                )
                if _integrity_requested(args):
                    sha256_hex, size_bytes, expected_hash = _resolve_artifact_integrity(args, args.out)
                    metadata = {
                        "rescan": bool(args.rescan),
                        "save_type": save_type_name(save_type),
                        "save_type_raw": save_type,
                        "exported_size_bytes": data_len,
                    }
                    if expected_hash is not None:
                        metadata["expected_sha256"] = expected_hash
                    _maybe_write_integrity_manifest(
                        sess,
                        args,
                        operation="n64-export",
                        direction="export",
                        artifact_path=args.out,
                        sha256_hex=sha256_hex,
                        size_bytes=size_bytes,
                        crc16_u16=host_crc,
                        metadata=metadata,
                    )

            elif args.sub == "n64-sram-raw-export":
                data_len, host_crc = n64_export_sram_raw(
                    sess,
                    args.out,
                    size_bytes=args.size,
                    rescan=args.rescan,
                )
                print(
                    f"Exported raw SRAM window ({data_len} bytes) -> {args.out}, "
                    f"CRC16=0x{host_crc:04X}"
                )
                if _integrity_requested(args):
                    sha256_hex, size_bytes, expected_hash = _resolve_artifact_integrity(args, args.out)
                    metadata = {
                        "rescan": bool(args.rescan),
                        "requested_size_bytes": int(args.size),
                        "exported_size_bytes": data_len,
                    }
                    if expected_hash is not None:
                        metadata["expected_sha256"] = expected_hash
                    _maybe_write_integrity_manifest(
                        sess,
                        args,
                        operation="n64-sram-raw-export",
                        direction="export",
                        artifact_path=args.out,
                        sha256_hex=sha256_hex,
                        size_bytes=size_bytes,
                        crc16_u16=host_crc,
                        metadata=metadata,
                    )

            elif args.sub == "n64-import":
                input_sha256 = None
                input_size = None
                expected_hash = None
                if _integrity_requested(args):
                    input_sha256, input_size, expected_hash = _resolve_artifact_integrity(args, args.input_file)

                save_type, host_crc = n64_import_save(
                    sess, args.input_file, rescan=args.rescan, verify=args.verify
                )
                print(
                    f"Imported N64 save ({save_type_name(save_type)}) from {args.input_file}, "
                    f"CRC16=0x{host_crc:04X}"
                )
                if _integrity_requested(args):
                    if input_sha256 is None or input_size is None:
                        input_sha256, input_size, expected_hash = _resolve_artifact_integrity(args, args.input_file)
                    metadata = {
                        "rescan": bool(args.rescan),
                        "verify_readback": bool(args.verify),
                        "save_type": save_type_name(save_type),
                        "save_type_raw": save_type,
                    }
                    if expected_hash is not None:
                        metadata["expected_sha256"] = expected_hash
                    _maybe_write_integrity_manifest(
                        sess,
                        args,
                        operation="n64-import",
                        direction="import",
                        artifact_path=args.input_file,
                        sha256_hex=input_sha256,
                        size_bytes=input_size,
                        crc16_u16=host_crc,
                        metadata=metadata,
                    )

            elif args.sub == "n64-gs-export":
                gs_size, host_crc = n64_gs_export(sess, args.out)
                print(
                    f"Exported GameShark ({gs_size} bytes) -> {args.out}, "
                    f"CRC16=0x{host_crc:04X}"
                )
                if _integrity_requested(args):
                    sha256_hex, size_bytes, expected_hash = _resolve_artifact_integrity(args, args.out)
                    metadata = {
                        "exported_size_bytes": gs_size,
                    }
                    if expected_hash is not None:
                        metadata["expected_sha256"] = expected_hash
                    _maybe_write_integrity_manifest(
                        sess,
                        args,
                        operation="n64-gs-export",
                        direction="export",
                        artifact_path=args.out,
                        sha256_hex=sha256_hex,
                        size_bytes=size_bytes,
                        crc16_u16=host_crc,
                        metadata=metadata,
                    )

            elif args.sub == "n64-gs-import":
                gs_size, host_crc = n64_gs_import(sess, args.input_file)
                print(
                    f"Imported GameShark ({gs_size} bytes) <- {args.input_file}, "
                    f"CRC16=0x{host_crc:04X}"
                )

            elif args.sub == "n64-gs-info":
                info = n64_gs_info(sess)
                print("GameShark Detection Info:")
                for k, v in info.items():
                    if isinstance(v, int) and k in ("probe_status", "flash_id", "mfg_id", "caps", "flags"):
                        print(f"  {k}: 0x{v:04X}")
                    elif k == "base_addr":
                        print(f"  {k}: 0x{v:08X}")
                    else:
                        print(f"  {k}: {v}")

            elif args.sub == "n64-controller-probe":
                sess.cfg.quiet = True
                device, status = n64_controller_probe(sess)
                print(
                    f"N64 controller probe: device=0x{device:04X}, "
                    f"status=0x{status:02X}"
                )

            elif args.sub == "n64-controller-poll":
                sess.cfg.quiet = True
                buttons, stick_x, stick_y = n64_controller_poll(sess, rumble=args.rumble)
                pressed = n64_controller_button_names(buttons)
                pressed_text = ", ".join(pressed) if pressed else "(none)"
                b0 = (buttons >> 8) & 0xFF
                b1 = buttons & 0xFF
                print(
                    f"N64 controller: buttons=0x{buttons:04X} (b0=0x{b0:02X}, b1=0x{b1:02X}) [{pressed_text}], "
                    f"stick=({stick_x},{stick_y})"
                )

            elif args.sub == "n64-controller-test":
                sess.cfg.quiet = True
                hz = max(1, args.hz)
                delay_s = 1.0 / hz
                device, status = n64_controller_probe(sess)
                print(
                    f"Controller detected: device=0x{device:04X}, status=0x{status:02X}. "
                    f"Polling at {hz} Hz (Ctrl+C to stop)."
                )

                last = None
                while True:
                    buttons, stick_x, stick_y = n64_controller_poll(sess, rumble=args.rumble)
                    state = (buttons, stick_x, stick_y)
                    if state != last:
                        pressed = n64_controller_button_names(buttons)
                        pressed_text = ", ".join(pressed) if pressed else "(none)"
                        b0 = (buttons >> 8) & 0xFF
                        b1 = buttons & 0xFF
                        print(
                            f"buttons=0x{buttons:04X} (b0=0x{b0:02X}, b1=0x{b1:02X}) [{pressed_text}] "
                            f"stick=({stick_x},{stick_y})"
                        )
                        last = state
                    time.sleep(delay_s)

            elif args.sub == "n64-mpk-export":
                host_crc = n64_export_mpk(sess, args.out)
                print(f"Exported N64 MPK ({32768} bytes) -> {args.out}, CRC16=0x{host_crc:04X}")
                if _integrity_requested(args):
                    sha256_hex, size_bytes, expected_hash = _resolve_artifact_integrity(args, args.out)
                    metadata = {
                        "exported_size_bytes": 32768,
                        "media_type": "mpk",
                    }
                    if expected_hash is not None:
                        metadata["expected_sha256"] = expected_hash
                    _maybe_write_integrity_manifest(
                        sess,
                        args,
                        operation="n64-mpk-export",
                        direction="export",
                        artifact_path=args.out,
                        sha256_hex=sha256_hex,
                        size_bytes=size_bytes,
                        crc16_u16=host_crc,
                        metadata=metadata,
                    )

            elif args.sub == "n64-mpk-import":
                input_sha256 = None
                input_size = None
                expected_hash = None
                if _integrity_requested(args):
                    input_sha256, input_size, expected_hash = _resolve_artifact_integrity(args, args.input_file)

                host_crc = n64_import_mpk(sess, args.input_file, verify=args.verify)
                print(f"Imported N64 MPK from {args.input_file}, CRC16=0x{host_crc:04X}")
                if _integrity_requested(args):
                    if input_sha256 is None or input_size is None:
                        input_sha256, input_size, expected_hash = _resolve_artifact_integrity(args, args.input_file)
                    metadata = {
                        "verify_readback": bool(args.verify),
                        "media_type": "mpk",
                    }
                    if expected_hash is not None:
                        metadata["expected_sha256"] = expected_hash
                    _maybe_write_integrity_manifest(
                        sess,
                        args,
                        operation="n64-mpk-import",
                        direction="import",
                        artifact_path=args.input_file,
                        sha256_hex=input_sha256,
                        size_bytes=input_size,
                        crc16_u16=host_crc,
                        metadata=metadata,
                    )

            else:
                raise MagicCliError(f"Unsupported subcommand: {args.sub}")

    except (MagicCliError, OSError, ValueError, KeyboardInterrupt) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    return 0
