from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import string
import subprocess
from typing import Any


def sha256_file(path: str | Path, chunk_size: int = 1024 * 1024) -> str:
    h = hashlib.sha256()
    p = Path(path)
    with p.open("rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def normalize_sha256_hex(value: str) -> str:
    cleaned = value.strip().lower()
    if cleaned.startswith("0x"):
        cleaned = cleaned[2:]
    if len(cleaned) != 64 or any(ch not in string.hexdigits.lower() for ch in cleaned):
        raise ValueError(f"Expected 64 hex chars for SHA256, got '{value}'")
    return cleaned


def _git_cmd(repo_root: Path, *args: str) -> str | None:
    try:
        proc = subprocess.run(
            ["git", "-C", str(repo_root), *args],
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    return proc.stdout.strip() or None


def host_git_metadata(repo_root: Path | None = None) -> dict[str, Any]:
    root = repo_root or Path(__file__).resolve().parents[2]
    return {
        "repo_root": str(root),
        "git_commit": _git_cmd(root, "rev-parse", "--short", "HEAD"),
        "git_commit_full": _git_cmd(root, "rev-parse", "HEAD"),
        "git_describe": _git_cmd(root, "describe", "--always", "--dirty", "--tags"),
    }


def timestamp_utc_now() -> str:
    return (
        datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z")
    )


def build_artifact_manifest(
    *,
    operation: str,
    direction: str,
    artifact_path: str | Path,
    artifact_size: int,
    sha256_hex: str,
    crc16_u16: int | None,
    firmware_version: str | None,
    serial_port: str | None,
    metadata: dict[str, Any] | None = None,
) -> dict[str, Any]:
    artifact_abs = str(Path(artifact_path).expanduser().resolve())
    manifest: dict[str, Any] = {
        "schema_version": 1,
        "timestamp_utc": timestamp_utc_now(),
        "operation": operation,
        "direction": direction,
        "artifact": {
            "path": artifact_abs,
            "size_bytes": int(artifact_size),
            "sha256": sha256_hex.lower(),
        },
        "device": {
            "port": serial_port,
            "firmware_version": firmware_version,
        },
        "host": host_git_metadata(),
    }
    if crc16_u16 is not None:
        manifest["artifact"]["crc16"] = f"0x{crc16_u16 & 0xFFFF:04X}"
    if metadata:
        manifest["metadata"] = metadata
    return manifest


def write_manifest(path: str | Path, manifest: dict[str, Any]) -> str:
    out_path = Path(path).expanduser().resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write("\n")
    return str(out_path)
