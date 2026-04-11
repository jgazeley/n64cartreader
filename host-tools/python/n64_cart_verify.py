#!/usr/bin/env python3
"""Focused single-target verification for one cart type or controller/MPK.

Fills the gap between gate_runner (full sequence across all targets) and
soak_test (single repeated operation). This script runs ALL operations
for ONE target: probe, export stability, import+verify, re-export confirm.

Replaces n64_validation_matrix.py which was a weaker subset of gate_runner.

Examples:
    # Test EEPROM cart (GoldenEye) thoroughly
    python3 tools/n64_cart_verify.py --target eeprom --ref ~/saves/GOLDENEYE.eep --iters 10

    # Test SRAM cart (Ocarina) with 5 iterations
    python3 tools/n64_cart_verify.py --target sram --ref ~/saves/OOT.sra

    # Test MPK roundtrip
    python3 tools/n64_cart_verify.py --target mpk --ref ~/saves/Mempak.mpk --iters 20

    # Just probe controller (no ref file needed)
    python3 tools/n64_cart_verify.py --target controller
"""

from __future__ import annotations

import argparse
import hashlib
import os
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path


CLI_PATH = Path(__file__).resolve().with_name("pico_pak_n64_magic_cli.py")

# Reference file size → expected firmware save type string.
# Used to auto-detect eeprom4k vs eeprom16k without an extra flag.
EEPROM_SIZE_TO_TYPE = {
    512: "eeprom4k",
    2048: "eeprom16k",
}

TARGET_EXPECTED_TYPES = {
    "sram": "sram",
    "flash": "flashram",
    # eeprom is resolved at runtime from ref file size
}


@dataclass
class StepResult:
    name: str
    status: str  # PASS, FAIL, PRECONDITION, SKIP
    info: str


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def run_cli(port: str | None, cli_args: list[str], timeout: int = 120) -> tuple[bool, str, str]:
    cmd = [sys.executable, str(CLI_PATH)]
    if port:
        cmd += ["--port", port]
    cmd += cli_args
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return p.returncode == 0, p.stdout.strip(), p.stderr.strip()
    except subprocess.TimeoutExpired:
        return False, "", "Command timed out"
    except Exception as e:
        return False, "", str(e)


def looks_like_precondition(err: str) -> bool:
    needles = (
        "controller/mempak not detected",
        "N64 save not available",
        "No cart save detected",
        "N64 save probe conflict",
        "IO_ERR",
    )
    return any(n in err for n in needles)


def classify(ok: bool, err: str) -> str:
    if ok:
        return "PASS"
    return "PRECONDITION" if looks_like_precondition(err) else "FAIL"


def resolve_eeprom_type(ref_path: Path) -> str | None:
    """Infer eeprom4k vs eeprom16k from reference file size."""
    size = ref_path.stat().st_size
    return EEPROM_SIZE_TO_TYPE.get(size)


# ---------------------------------------------------------------------------
# Export stability loop (shared by cart and MPK paths)
# ---------------------------------------------------------------------------

def export_stability(
    *,
    label: str,
    port: str | None,
    export_cmd: list[str],
    ref_path: Path,
    iters: int,
    tmp_dir: Path,
) -> StepResult:
    ref_hash = sha256_file(ref_path)
    ref_size = ref_path.stat().st_size

    for i in range(1, iters + 1):
        out_file = tmp_dir / f"{label}_{i:03d}.bin"
        ok, out, err = run_cli(port, export_cmd + ["--out", str(out_file)], timeout=300)
        if not ok:
            reason = err or out
            status = "PRECONDITION" if looks_like_precondition(reason) else "FAIL"
            return StepResult(f"{label} Export Stability", status, f"iter {i}: export failed: {reason}")

        if not out_file.exists():
            return StepResult(f"{label} Export Stability", "FAIL", f"iter {i}: file missing after export")

        got_size = out_file.stat().st_size
        got_hash = sha256_file(out_file)

        if got_size != ref_size:
            return StepResult(f"{label} Export Stability", "FAIL",
                              f"iter {i}: size mismatch got={got_size} expected={ref_size}")

        if got_hash != ref_hash:
            evidence = tmp_dir / f"FAIL_{label}_{i:03d}.bin"
            out_file.replace(evidence)
            return StepResult(f"{label} Export Stability", "FAIL",
                              f"iter {i}: hash mismatch (evidence: {evidence})")

        out_file.unlink(missing_ok=True)
        print(f"  [{i:03d}/{iters:03d}] PASS hash={got_hash[:16]}...")

    return StepResult(f"{label} Export Stability", "PASS",
                      f"{iters}/{iters} exports match reference ({ref_size} bytes)")


# ---------------------------------------------------------------------------
# Import + verify + re-export confirm
# ---------------------------------------------------------------------------

def import_verify_reexport(
    *,
    label: str,
    port: str | None,
    import_cmd: list[str],
    export_cmd: list[str],
    ref_path: Path,
    tmp_dir: Path,
    use_verify_flag: bool = True,
) -> list[StepResult]:
    results = []
    ref_hash = sha256_file(ref_path)

    # Import with --verify (firmware readback compare)
    if use_verify_flag:
        print(f"\n  Importing {ref_path.name} with --verify...")
        cmd_args = import_cmd + ["--verify"]
    else:
        print(f"\n  Importing {ref_path.name}...")
        cmd_args = import_cmd
    ok, out, err = run_cli(port, cmd_args, timeout=420)
    status = classify(ok, err)
    info = (out.splitlines()[-1] if out else "ok") if ok else (err or out)
    results.append(StepResult(f"{label} Import+Verify", status, info))
    if ok:
        print(f"  {info}")
    else:
        print(f"  FAILED: {info}")
        return results  # no point re-exporting if import failed

    # Re-export and compare hash to confirm import didn't corrupt
    print(f"  Re-exporting to confirm integrity...")
    reexport_file = tmp_dir / f"{label}_reexport.bin"
    ok, out, err = run_cli(port, export_cmd + ["--out", str(reexport_file)], timeout=300)
    if not ok:
        reason = err or out
        results.append(StepResult(f"{label} Re-export Confirm", classify(ok, reason), f"re-export failed: {reason}"))
        return results

    if not reexport_file.exists():
        results.append(StepResult(f"{label} Re-export Confirm", "FAIL", "file missing after re-export"))
        return results

    got_hash = sha256_file(reexport_file)
    reexport_file.unlink(missing_ok=True)

    if got_hash == ref_hash:
        results.append(StepResult(f"{label} Re-export Confirm", "PASS",
                                  f"hash matches reference after import"))
        print(f"  Re-export hash matches reference.")
    else:
        results.append(StepResult(f"{label} Re-export Confirm", "FAIL",
                                  f"hash mismatch after import: got {got_hash[:16]}... expected {ref_hash[:16]}..."))
        print(f"  HASH MISMATCH after import!")

    return results


# ---------------------------------------------------------------------------
# Target-specific test flows
# ---------------------------------------------------------------------------

def verify_cart(target: str, port: str | None, ref_path: Path, iters: int, tmp_dir: Path) -> list[StepResult]:
    """Run full verification for a cart save type (eeprom/sram/flash)."""
    results = []
    label = target.upper()

    # Determine expected type
    if target == "eeprom":
        exp_type = resolve_eeprom_type(ref_path)
        if exp_type is None:
            results.append(StepResult(f"{label} Type Detect", "FAIL",
                                      f"Cannot infer EEPROM type from ref size {ref_path.stat().st_size} "
                                      f"(expected 512 for 4k or 2048 for 16k)"))
            return results
        print(f"  EEPROM type inferred from ref size: {exp_type}")
    else:
        exp_type = TARGET_EXPECTED_TYPES[target]
    exp_size = ref_path.stat().st_size

    # Step 1: Rescan
    print(f"\n[{label}] Rescan...")
    ok, out, err = run_cli(port, ["n64-rescan"], timeout=90)
    if not ok:
        results.append(StepResult(f"{label} Rescan", classify(ok, err), err or out))
        return results
    results.append(StepResult(f"{label} Rescan", "PASS", "ok"))

    # Step 2: Status check — validate type and size
    print(f"[{label}] Status check...")
    ok, out, err = run_cli(port, ["n64-status"], timeout=90)
    if not ok:
        results.append(StepResult(f"{label} Status", classify(ok, err), err or out))
        return results

    # Parse type and size from status output
    import re
    m = re.search(r"type=([a-zA-Z0-9_]+)\s+\(\d+\),\s+size=(\d+)\s+bytes", out)
    if not m:
        results.append(StepResult(f"{label} Status", "FAIL", f"Could not parse status: {out}"))
        return results

    got_type = m.group(1).lower()
    got_size = int(m.group(2))

    if got_type != exp_type or got_size != exp_size:
        results.append(StepResult(f"{label} Status", "FAIL",
                                  f"got type={got_type} size={got_size}, expected type={exp_type} size={exp_size}"))
        return results

    results.append(StepResult(f"{label} Status", "PASS", f"type={got_type} size={got_size}"))
    print(f"  type={got_type} size={got_size} — matches expected")

    # Step 3: Export stability
    print(f"\n[{label}] Export stability ({iters} iterations)...")
    results.append(export_stability(
        label=label, port=port, export_cmd=["n64-export"],
        ref_path=ref_path, iters=iters, tmp_dir=tmp_dir,
    ))
    if results[-1].status != "PASS":
        return results

    # Step 4: Import + verify + re-export
    print(f"\n[{label}] Import + verify + re-export...")
    results.extend(import_verify_reexport(
        label=label, port=port,
        import_cmd=["n64-import", "--in", str(ref_path)],
        export_cmd=["n64-export"],
        ref_path=ref_path, tmp_dir=tmp_dir,
    ))

    return results


def verify_mpk(port: str | None, ref_path: Path, iters: int, tmp_dir: Path) -> list[StepResult]:
    """Run full verification for controller mempak."""
    results = []

    # Step 1: Controller probe
    print("\n[MPK] Controller probe...")
    ok, out, err = run_cli(port, ["n64-controller-probe"], timeout=60)
    status = classify(ok, err)
    results.append(StepResult("Controller Probe", status, out if ok else (err or out)))
    if ok:
        print(f"  {out}")
    else:
        print(f"  {err or out}")
        if status == "PRECONDITION":
            return results  # no controller, can't continue

    # Step 2: MPK export stability
    print(f"\n[MPK] Export stability ({iters} iterations)...")
    results.append(export_stability(
        label="MPK", port=port, export_cmd=["n64-mpk-export"],
        ref_path=ref_path, iters=iters, tmp_dir=tmp_dir,
    ))
    if results[-1].status != "PASS":
        return results

    # Step 3: MPK import + verify + re-export
    print(f"\n[MPK] Import + verify + re-export...")
    results.extend(import_verify_reexport(
        label="MPK", port=port,
        import_cmd=["n64-mpk-import", "--in", str(ref_path)],
        export_cmd=["n64-mpk-export"],
        ref_path=ref_path, tmp_dir=tmp_dir,
    ))

    return results


def verify_controller(port: str | None) -> list[StepResult]:
    """Probe controller only (no MPK operations, no ref file needed)."""
    results = []

    print("\n[CONTROLLER] Probe...")
    ok, out, err = run_cli(port, ["n64-controller-probe"], timeout=60)
    status = classify(ok, err)
    results.append(StepResult("Controller Probe", status, out if ok else (err or out)))
    if ok:
        print(f"  {out}")
    else:
        print(f"  {err or out}")

    return results


def verify_gameshark(port: str | None, ref_path: Path, iters: int, tmp_dir: Path) -> list[StepResult]:
    """Run full verification for GameShark flash."""
    results = []

    # Step 1: GS Info
    print("\n[GS] Info...")
    ok, out, err = run_cli(port, ["n64-gs-info"], timeout=60)
    if not ok:
        results.append(StepResult("GS Info", classify(ok, err), err or out))
        return results

    # Simple check for 'Present      : Yes' in info output
    if "Present      : Yes" not in out:
        results.append(StepResult("GS Info", "FAIL", f"GameShark not detected: {out}"))
        return results

    results.append(StepResult("GS Info", "PASS", out))
    print(f"  {out}")

    # Step 2: GS export stability
    print(f"\n[GS] Export stability ({iters} iterations)...")
    results.append(export_stability(
        label="GS", port=port, export_cmd=["n64-gs-export"],
        ref_path=ref_path, iters=iters, tmp_dir=tmp_dir,
    ))
    if results[-1].status != "PASS":
        return results

    # Step 3: GS import + verify + re-export
    print(f"\n[GS] Import + verify + re-export...")
    results.extend(import_verify_reexport(
        label="GS", port=port,
        import_cmd=["n64-gs-import", "--in", str(ref_path)],
        export_cmd=["n64-gs-export"],
        ref_path=ref_path, tmp_dir=tmp_dir,
        use_verify_flag=False,
    ))

    return results


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Focused single-target N64 verification (probe + export + import + re-export)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
targets:
  eeprom      EEPROM cart (auto-detects 4k/16k from ref file size)
  sram        SRAM cart
  flash       FlashRAM cart
  mpk         Controller mempak (requires controller plugged in)
  controller  Controller probe only (no ref file needed)
  gameshark   GameShark flash (requires controller unplugged)
""",
    )
    parser.add_argument("--target", required=True,
                        choices=["eeprom", "sram", "flash", "mpk", "controller", "gameshark"],
                        help="What to test")
    parser.add_argument("--ref", help="Reference file for hash comparison (required for all except controller)")
    parser.add_argument("--port", help="Serial port (e.g. /dev/ttyACM0, auto-detected if omitted)")
    parser.add_argument("--iters", type=int, default=5, help="Export stability iterations (default: 5)")
    args = parser.parse_args()

    # Validate ref file requirement
    needs_ref = args.target != "controller"
    if needs_ref and not args.ref:
        parser.error(f"--ref is required for target '{args.target}'")
    if args.ref and not Path(args.ref).exists():
        print(f"[FATAL] Reference file not found: {args.ref}")
        return 3

    if not CLI_PATH.exists():
        print(f"[FATAL] CLI not found: {CLI_PATH}")
        return 3

    ref_path = Path(args.ref) if args.ref else None

    print(f"=== N64 Cart Verify: {args.target.upper()} ===")
    print(f"Port: {args.port or '(auto-detect)'} | Iterations: {args.iters}")
    if ref_path:
        print(f"Reference: {ref_path} ({ref_path.stat().st_size} bytes)")
    print("=" * 60)

    # Firmware identity check (non-fatal for older firmware that lacks fw-version)
    print("\n[0] Firmware identity...")
    ok, out, err = run_cli(args.port, ["fw-version"], timeout=60)
    if ok:
        fw_line = out.splitlines()[-1] if out else "ok"
        print(f"  {fw_line}")
    else:
        print("  fw-version unavailable, falling back to ping...")
        ok, out, err = run_cli(args.port, ["ping"], timeout=30)
        if not ok:
            print(f"  Ping FAILED: {err or out}")
            print("\nCannot proceed without firmware connection.")
            return 1
        print("  Ping OK")

    start_time = time.time()

    with tempfile.TemporaryDirectory(prefix="n64_verify_") as td:
        tmp_dir = Path(td)

        if args.target in ("eeprom", "sram", "flash"):
            results = verify_cart(args.target, args.port, ref_path, args.iters, tmp_dir)
        elif args.target == "mpk":
            results = verify_mpk(args.port, ref_path, args.iters, tmp_dir)
        elif args.target == "controller":
            results = verify_controller(args.port)
        elif args.target == "gameshark":
            results = verify_gameshark(args.port, ref_path, args.iters, tmp_dir)

    duration = time.time() - start_time

    # Summary
    print("\n" + "=" * 60)
    print(f"VERIFY SUMMARY: {args.target.upper()} ({duration:.1f}s)")
    print("=" * 60)
    for r in results:
        print(f"{r.status:12} | {r.name:28} | {r.info}")

    hard_fail = any(r.status == "FAIL" for r in results)
    precond = any(r.status == "PRECONDITION" for r in results)

    if hard_fail:
        print("\nVERDICT: FAIL")
        return 1
    elif precond:
        print("\nVERDICT: PRECONDITION (fix setup and retry)")
        return 2
    else:
        print("\nVERDICT: PASS")
        return 0


if __name__ == "__main__":
    sys.exit(main())
