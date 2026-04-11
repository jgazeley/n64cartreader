#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from pico_pak.n64_ops import (  # noqa: E402
    n64_export_header,
    n64_export_save_bytes,
    n64_repro_rom_diagnostic,
    n64_rescan_and_configure,
    n64_rom_info,
    n64_status,
    pico_firmware_version,
    save_type_name,
)
from pico_pak.protocol import MagicCliError  # noqa: E402
from pico_pak.transport import Cfg, MagicSession  # noqa: E402


PORT_GLOB = "/dev/serial/by-id/usb-Raspberry_Pi_Pico_*if00"
SAVE_TYPE_NONE = 0
SAVE_TYPE_UNKNOWN = 5
SERIAL_RE = re.compile(r"usb-Raspberry_Pi_Pico_([A-Za-z0-9]+)-if00$")


@dataclass
class CheckResult:
    name: str
    status: str
    detail: str


@dataclass
class PortResult:
    port: str
    serial: str
    overall: str = "PASS"
    checks: list[CheckResult] = field(default_factory=list)
    metadata: dict[str, Any] = field(default_factory=dict)

    def add_check(self, name: str, status: str, detail: str) -> None:
        self.checks.append(CheckResult(name=name, status=status, detail=detail))
        if status == "FAIL":
            self.overall = "FAIL"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Regression harness for Pico Pak N64 bench validation",
    )
    parser.add_argument(
        "--port",
        action="append",
        dest="ports",
        help="Specific /dev/serial/by-id port(s) to validate. Defaults to all attached Pico by-id ports.",
    )
    parser.add_argument(
        "--repro-port",
        action="append",
        default=[],
        help="Port(s) that should also run the repro-only ROM diagnostic.",
    )
    parser.add_argument(
        "--run-repro-diagnostic-all",
        action="store_true",
        help="Run the repro-only ROM diagnostic on every port.",
    )
    parser.add_argument("--db", help="Optional cart DB JSON override passed to identify_cart")
    parser.add_argument("--catalog-txt", help="Optional n64.txt override passed to identify_cart")
    parser.add_argument("--json-out", help="Write full JSON result to this path")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--chunk-size", type=int, default=512)
    parser.add_argument("--retries", type=int, default=5)
    parser.add_argument("--quiet", action="store_true", help="Suppress per-check progress output")
    return parser.parse_args()


def discover_ports(explicit_ports: list[str] | None) -> list[str]:
    if explicit_ports:
        return sorted(str(Path(port)) for port in explicit_ports)
    return sorted(str(path) for path in Path("/dev/serial/by-id").glob("usb-Raspberry_Pi_Pico_*if00"))


def serial_from_port(port: str) -> str:
    match = SERIAL_RE.search(Path(port).name)
    return match.group(1) if match else "unknown"


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def format_match_detail(result: dict[str, Any] | None) -> str:
    if not result:
        return "no identification result"
    fingerprint = result.get("fingerprint", {})
    matched = result.get("matched") or {}
    return (
        f"match={result.get('match_level', 'unknown')} "
        f"title={fingerprint.get('title', '?')} "
        f"game_id={fingerprint.get('game_id', '?')} "
        f"rom_size={fingerprint.get('rom_size', 0)} "
        f"matched={matched.get('slug', 'none')}"
    )


def wants_repro_diag(port: str, args: argparse.Namespace) -> bool:
    return args.run_repro_diagnostic_all or port in set(args.repro_port)


def validate_port(port: str, args: argparse.Namespace) -> PortResult:
    result = PortResult(port=port, serial=serial_from_port(port))
    cfg = Cfg(
        port=port,
        timeout=args.timeout,
        chunk_size=args.chunk_size,
        retries=args.retries,
        quiet=True,
    )

    try:
        with MagicSession(cfg) as sess:
            fw = pico_firmware_version(sess)
            result.metadata["firmware_version"] = fw
            result.add_check("fw-version", "PASS", fw)

            cart = n64_rescan_and_configure(
                sess,
                db_path=args.db,
                catalog_txt_path=args.catalog_txt,
            )
            result.metadata["cart_id"] = cart
            result.add_check("n64-cart-id --rescan", "PASS", format_match_detail(cart))

            header, header_crc = n64_export_header(sess, rescan=False)
            prefix = header[:4].hex().upper()
            result.metadata["header_crc16"] = f"0x{header_crc:04X}"
            result.metadata["header_prefix"] = prefix
            result.add_check("n64-header --rescan", "PASS", f"crc16=0x{header_crc:04X} prefix={prefix}")

            rom_size, units2k = n64_rom_info(sess, rescan=False)
            result.metadata["rom_size_bytes"] = rom_size
            result.metadata["rom_size_units2k"] = units2k
            result.add_check("n64-rom-info --rescan", "PASS", f"{rom_size} bytes ({units2k} units2k)")

            save_type, save_size, _meta = n64_status(sess, rescan=False)
            save_name = save_type_name(save_type)
            result.metadata["save_type"] = save_name
            result.metadata["save_size_bytes"] = save_size

            if save_size > 0 and save_type not in (SAVE_TYPE_NONE, SAVE_TYPE_UNKNOWN):
                save_type_1, data1, save_size_1, crc1 = n64_export_save_bytes(sess, rescan=False)
                save_type_2, data2, save_size_2, crc2 = n64_export_save_bytes(sess, rescan=False)
                hash1 = sha256_hex(data1)
                hash2 = sha256_hex(data2)
                same = (data1 == data2)
                detail = (
                    f"type={save_type_name(save_type_1)} size={save_size_1} "
                    f"sha256_1={hash1} sha256_2={hash2} "
                    f"crc16_1=0x{crc1:04X} crc16_2=0x{crc2:04X}"
                )
                result.metadata["save_repeatability"] = {
                    "type_1": save_type_name(save_type_1),
                    "type_2": save_type_name(save_type_2),
                    "size_1": save_size_1,
                    "size_2": save_size_2,
                    "sha256_1": hash1,
                    "sha256_2": hash2,
                    "crc16_1": f"0x{crc1:04X}",
                    "crc16_2": f"0x{crc2:04X}",
                    "matched": same,
                }
                result.add_check("repeated save export", "PASS" if same else "FAIL", detail)
            else:
                result.add_check(
                    "repeated save export",
                    "SKIP",
                    f"save_type={save_name} size={save_size}",
                )

            if wants_repro_diag(port, args):
                diag = n64_repro_rom_diagnostic(sess)
                result.metadata["repro_diagnostic"] = {
                    "success": bool(diag.get("success")),
                    "samples": {
                        f"0x{offset:06X}": int(word)
                        for offset, word in diag.get("samples", {}).items()
                    },
                }
                result.add_check(
                    "n64-repro-rom-diagnostic",
                    "PASS" if diag.get("success") else "FAIL",
                    (
                        f"0x000000=0x{diag['samples'][0x000000]:04X} "
                        f"0x001000=0x{diag['samples'][0x001000]:04X} "
                        f"0x200000=0x{diag['samples'][0x200000]:04X}"
                    ),
                )
            else:
                result.add_check("n64-repro-rom-diagnostic", "SKIP", "not requested")

    except Exception as exc:
        result.add_check("session", "FAIL", str(exc))

    return result


def print_result(result: PortResult, quiet: bool) -> None:
    if quiet:
        return
    print(f"\n=== {result.port} ({result.serial}) ===")
    print(f"OVERALL: {result.overall}")
    for check in result.checks:
        print(f"  [{check.status}] {check.name}: {check.detail}")


def emit_json(results: list[PortResult], json_out: str | None) -> None:
    payload = {
        "ts_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "results": [
            {
                "port": result.port,
                "serial": result.serial,
                "overall": result.overall,
                "checks": [
                    {
                        "name": check.name,
                        "status": check.status,
                        "detail": check.detail,
                    }
                    for check in result.checks
                ],
                "metadata": result.metadata,
            }
            for result in results
        ],
    }
    if json_out:
        out_path = Path(json_out)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    else:
        print(json.dumps(payload, indent=2, sort_keys=True))


def main() -> int:
    args = parse_args()
    ports = discover_ports(args.ports)
    if not ports:
        print("No Pico by-id ports found.", file=sys.stderr)
        return 1

    results = [validate_port(port, args) for port in ports]
    for result in results:
        print_result(result, args.quiet)

    total = len(results)
    passed = sum(1 for result in results if result.overall == "PASS")
    failed = total - passed
    if not args.quiet:
        print("\n=== Summary ===")
        print(f"ports={total} pass={passed} fail={failed}")

    if args.json_out:
        emit_json(results, args.json_out)

    return 0 if failed == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
