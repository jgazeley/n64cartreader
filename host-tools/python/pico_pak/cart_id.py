from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import re
from typing import Any
import zlib

from .protocol import N64_HEADER_SIZE

_TITLE_START = 0x20
_TITLE_END = 0x34
_CRC1_OFF = 0x10
_CRC2_OFF = 0x14
_GAME_ID_START = 0x3B
_GAME_ID_END = 0x3F
_VERSION_OFF = 0x3F

_MATCH_RANK = {
    "unknown": 0,
    "weak": 1,
    "strong": 2,
    "exact": 3,
}

_N64TXT_META_RE = re.compile(
    r"^\s*([0-9A-Fa-f]{8})\s*,\s*([0-9A-Fa-f]{8})\s*,\s*([0-9A-Fa-f]{1,3})\s*,\s*([0-9A-Fa-f]{1,2})\s*$"
)

_N64TXT_SAVE_CODE_HINT = {
    # Inferred from observed retail matches + common reader conventions.
    0: "none",
    1: "sram",
    2: "sram96k",
    4: "flashram",
    5: "eeprom4k",
    6: "eeprom16k",
}

# Maps normalized save hint strings to (firmware_save_type_enum, default_size_bytes).
# Used by host-directed save configuration (CMD_N64_SET_SAVE_CFG).
_SAVE_HINT_TO_FW_CONFIG = {
    "none":      (0, 0),
    "sram":      (1, 32768),
    "sram96k":   (1, 98304),
    "flashram":  (2, 131072),
    "eeprom4k":  (3, 512),
    "eeprom16k": (4, 2048),
}


def save_hint_to_fw_config(hint: str | None) -> tuple[int, int] | None:
    """Convert a save_type_hint string to (fw_save_type, size_bytes), or None if unknown."""
    if hint is None:
        return None
    normalized = _normalize_save_hint(hint)
    if normalized is None:
        return None
    return _SAVE_HINT_TO_FW_CONFIG.get(normalized)


_GS_MAGIC = b"(C) MUSHROOM"
_GS_MAGIC_OFFSET = 0x20


def is_gameshark_header(header: bytes) -> bool:
    """Return True if the 64-byte ROM header belongs to a GameShark/Action Replay."""
    if len(header) < _GS_MAGIC_OFFSET + len(_GS_MAGIC):
        return False
    return header[_GS_MAGIC_OFFSET : _GS_MAGIC_OFFSET + len(_GS_MAGIC)] == _GS_MAGIC


_REGION_CODE_TO_NAME = {
    "A": "Asia",
    "B": "Brazil",
    "C": "China",
    "D": "Germany",
    "E": "North America",
    "F": "France",
    "G": "Gateway 64",
    "H": "Netherlands",
    "I": "Italy",
    "J": "Japan",
    "K": "Korea",
    "L": "Lodgenet",
    "N": "Canada",
    "P": "Europe",
    "S": "Spain",
    "U": "Australia",
    "W": "Scandinavia",
    "X": "Europe (alt)",
    "Y": "Europe (alt)",
}


def _default_catalog_path() -> Path:
    # Canonical location: host-tools/n64.txt (shared by Python CLI and Web UI).
    # This file lives at host-tools/python/pico_pak/cart_id.py, so
    # parents[2] is host-tools/.
    here = Path(__file__).resolve()
    return here.parents[2] / "n64.txt"


def _normalize_title(value: str) -> str:
    return " ".join(value.upper().replace("\x00", " ").split())


def _normalize_game_id(value: str) -> str:
    return "".join(ch for ch in value.upper() if ch.isalnum())


def _be32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "big")


def _ascii_clean(raw: bytes) -> str:
    chars = []
    for byte in raw:
        if 32 <= byte <= 126:
            chars.append(chr(byte))
        else:
            chars.append(" ")
    return "".join(chars)


def _is_printable_ascii(byte: int) -> bool:
    return 32 <= byte <= 126


def _normalize_save_hint(value: str | None) -> str | None:
    if value is None:
        return None
    raw = value.strip().lower()
    if not raw:
        return None

    if raw.startswith("n64txt_code_"):
        suffix = raw.split("_")[-1]
        if suffix.isdigit():
            mapped = _N64TXT_SAVE_CODE_HINT.get(int(suffix))
            if mapped:
                return mapped
        return raw

    compact = "".join(ch for ch in raw if ch.isalnum())
    aliases = {
        "none": "none",
        "sram": "sram",
        "sram96k": "sram96k",
        "flash": "flashram",
        "flashram": "flashram",
        "eeprom4k": "eeprom4k",
        "eeprom16k": "eeprom16k",
        "unknown": "unknown",
    }
    return aliases.get(compact, raw)


def _slugify(text: str) -> str:
    out = []
    last_underscore = False
    for ch in text.lower():
        if ch.isalnum():
            out.append(ch)
            last_underscore = False
        elif not last_underscore:
            out.append("_")
            last_underscore = True
    value = "".join(out).strip("_")
    return value or "untitled"


def _remove_rom_extension(title: str) -> str:
    for ext in (".z64", ".n64", ".v64"):
        if title.lower().endswith(ext):
            return title[: -len(ext)]
    return title


def _parse_small_int(text: str) -> int:
    stripped = text.strip()
    if any(ch in "ABCDEFabcdef" for ch in stripped):
        return int(stripped, 16)
    return int(stripped, 10)


def _parse_optional_int(value: Any, *, bits: int | None = None) -> int | None:
    if value is None:
        return None
    if isinstance(value, int):
        parsed = value
    elif isinstance(value, str):
        text = value.strip()
        if not text:
            return None
        parsed = int(text, 0)
    else:
        raise ValueError(f"Invalid integer field type: {type(value)!r}")

    if bits is not None:
        mask = (1 << bits) - 1
        parsed &= mask
    return parsed


def crc32_ieee_file(path: str | Path, chunk_size: int = 1024 * 1024) -> int:
    crc = 0
    p = Path(path)
    with p.open("rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            crc = zlib.crc32(chunk, crc)
    return crc & 0xFFFFFFFF


def _build_fingerprint(header: bytes, rom_size: int, rom_crc32_u32: int | None = None) -> dict[str, Any]:
    if len(header) != N64_HEADER_SIZE:
        raise ValueError(f"Expected {N64_HEADER_SIZE} header bytes, got {len(header)}")

    title = _ascii_clean(header[_TITLE_START:_TITLE_END]).rstrip(" \x00")
    game_id_raw = header[_GAME_ID_START:_GAME_ID_END]
    game_id = _ascii_clean(game_id_raw).strip()
    game_id_hex = game_id_raw.hex().upper()
    version = header[_VERSION_OFF]
    crc1 = _be32(header, _CRC1_OFF)
    crc2 = _be32(header, _CRC2_OFF)

    if _is_printable_ascii(game_id_raw[3]):
        region_code = chr(game_id_raw[3]).upper()
    else:
        region_code = "?"
    region_name = _REGION_CODE_TO_NAME.get(region_code, "Unknown")

    return {
        "title": title,
        "title_norm": _normalize_title(title),
        "game_id": game_id,
        "game_id_hex": game_id_hex,
        "game_id_norm": _normalize_game_id(game_id),
        "version": version,
        "rom_size": int(rom_size),
        "crc1_u32": crc1,
        "crc2_u32": crc2,
        "rom_crc32_u32": rom_crc32_u32,
        "crc1": f"0x{crc1:08X}",
        "crc2": f"0x{crc2:08X}",
        "rom_crc32_ieee": f"0x{rom_crc32_u32:08X}" if rom_crc32_u32 is not None else None,
        "region_code": region_code,
        "region_name": region_name,
    }


def _load_db(db_path: str | Path) -> tuple[Path, int, list[dict[str, Any]]]:
    path = Path(db_path).expanduser()
    path = path.resolve()

    with path.open("r", encoding="utf-8") as f:
        raw = json.load(f)

    schema_version = int(raw.get("schema_version", 0))
    entries_raw = raw.get("entries", [])
    if not isinstance(entries_raw, list):
        raise ValueError(f"DB entries must be a list: {path}")

    entries: list[dict[str, Any]] = []
    for item in entries_raw:
        if isinstance(item, dict):
            entries.append(item)
    return path, schema_version, entries


def _load_n64txt_catalog(catalog_txt_path: str | Path | None) -> tuple[Path | None, list[dict[str, Any]]]:
    if catalog_txt_path is None:
        path = _default_catalog_path()
        if not path.exists():
            return None, []
    else:
        path = Path(catalog_txt_path).expanduser()
        if not path.exists():
            raise FileNotFoundError(f"N64 text catalog not found: {path}")

    path = path.resolve()

    entries: list[dict[str, Any]] = []
    current_title: str | None = None
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for lineno, raw_line in enumerate(f, start=1):
            line = raw_line.strip()
            if not line:
                continue

            meta = _N64TXT_META_RE.match(line)
            if meta is not None:
                if not current_title:
                    continue
                field0 = int(meta.group(1), 16)
                crc1 = int(meta.group(2), 16)
                size_mib = _parse_small_int(meta.group(3))
                save_code = _parse_small_int(meta.group(4))
                if size_mib <= 0:
                    continue

                title = _remove_rom_extension(current_title).strip()
                slug = f"n64txt_{_slugify(title)}_{crc1:08x}_{size_mib}m"

                entries.append(
                    {
                        "slug": slug,
                        "title": title,
                        "rom_size": size_mib * 1024 * 1024,
                        "crc1": f"0x{crc1:08X}",
                        "rom_crc32_ieee": f"0x{field0:08X}",
                        "save_type_hint": _N64TXT_SAVE_CODE_HINT.get(save_code, f"n64txt_code_{save_code}"),
                        "source": "n64txt",
                        "catalog_line": lineno,
                        "catalog_path": str(path),
                        "n64txt_save_code": save_code,
                        "notes": f"Imported from n64.txt line {lineno}",
                    }
                )
                continue

            # Treat non-metadata lines as title lines.
            current_title = line

    return path, entries


def _candidate_from_entry(fingerprint: dict[str, Any], entry: dict[str, Any]) -> dict[str, Any] | None:
    entry_title = str(entry.get("title", "")).strip()
    entry_game_id = str(entry.get("game_id", "")).strip()
    title_match = bool(entry_title and _normalize_title(entry_title) == fingerprint["title_norm"])
    game_id_match = bool(entry_game_id and _normalize_game_id(entry_game_id) == fingerprint["game_id_norm"])

    entry_size = _parse_optional_int(entry.get("rom_size"))
    size_match = entry_size is None or fingerprint["rom_size"] == 0 or entry_size == fingerprint["rom_size"]

    entry_crc1 = _parse_optional_int(entry.get("crc1"), bits=32)
    entry_crc2 = _parse_optional_int(entry.get("crc2"), bits=32)
    entry_rom_crc32 = _parse_optional_int(
        entry.get("rom_crc32_ieee", entry.get("rom_crc32", entry.get("n64txt_field0"))),
        bits=32,
    )
    crc1_match = entry_crc1 is not None and entry_crc1 == fingerprint["crc1_u32"]
    crc2_match = entry_crc2 is not None and entry_crc2 == fingerprint["crc2_u32"]
    rom_crc32_match = (
        entry_rom_crc32 is not None
        and fingerprint["rom_crc32_u32"] is not None
        and entry_rom_crc32 == fingerprint["rom_crc32_u32"]
    )
    crc_pair_present = entry_crc1 is not None and entry_crc2 is not None
    crc_pair_match = crc_pair_present and crc1_match and crc2_match

    if (crc_pair_match and size_match) or (rom_crc32_match and crc1_match and size_match):
        level = "exact"
    elif (crc_pair_match and not size_match) or (crc1_match and size_match) or (game_id_match and size_match):
        level = "strong"
    elif rom_crc32_match or crc1_match or game_id_match or title_match:
        level = "weak"
    else:
        return None

    reason_parts = []
    if crc_pair_match:
        reason_parts.append("CRC1/CRC2 match")
    elif crc1_match:
        reason_parts.append("CRC1 match")
    if rom_crc32_match:
        reason_parts.append("ROM CRC32 match")
    if game_id_match:
        reason_parts.append("game_id match")
    if title_match:
        reason_parts.append("title match")
    if size_match and entry_size is not None:
        reason_parts.append("rom_size match")

    candidate = {
        "slug": str(entry.get("slug", "")).strip() or "(unnamed)",
        "title": entry_title or None,
        "game_id": entry_game_id or None,
        "region": entry.get("region"),
        "save_type_hint": entry.get("save_type_hint"),
        "source": entry.get("source", "db"),
        "catalog_line": entry.get("catalog_line"),
        "rom_size": entry_size,
        "crc1": f"0x{entry_crc1:08X}" if entry_crc1 is not None else None,
        "crc2": f"0x{entry_crc2:08X}" if entry_crc2 is not None else None,
        "rom_crc32_ieee": f"0x{entry_rom_crc32:08X}" if entry_rom_crc32 is not None else None,
        "notes": entry.get("notes"),
        "match_level": level,
        "reason": ", ".join(reason_parts) if reason_parts else "heuristic title/game_id match",
        "_rank": _MATCH_RANK[level],
        "_rom_crc32_match": 1 if rom_crc32_match else 0,
        "_has_crc_pair": 1 if crc_pair_present else 0,
        "_has_rom_crc32": 1 if entry_rom_crc32 is not None else 0,
        "_has_size": 1 if entry_size is not None else 0,
    }
    return candidate


def _finalize_candidate(candidate: dict[str, Any]) -> dict[str, Any]:
    return {k: v for k, v in candidate.items() if not k.startswith("_")}


def _append_unknown_signature(path: str | Path, fingerprint: dict[str, Any]) -> str:
    out_path = Path(path).expanduser()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "ts_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "fingerprint": {
            "title": fingerprint["title"],
            "game_id": fingerprint["game_id"],
            "game_id_hex": fingerprint["game_id_hex"],
            "version": fingerprint["version"],
            "rom_size": fingerprint["rom_size"],
            "crc1": fingerprint["crc1"],
            "crc2": fingerprint["crc2"],
            "region_code": fingerprint["region_code"],
        },
    }
    with out_path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(payload, sort_keys=True) + "\n")
    return str(out_path.resolve())


def identify_cart(
    header: bytes,
    rom_size: int,
    *,
    db_path: str | Path | None = None,
    catalog_txt_path: str | Path | None = None,
    use_catalog_txt: bool = True,
    rom_crc32_u32: int | None = None,
    observed_save_type: str | None = None,
    emit_unknown_path: str | Path | None = None,
) -> dict[str, Any]:
    fingerprint = _build_fingerprint(header, rom_size, rom_crc32_u32=rom_crc32_u32)

    # Early detection: GameShark / Action Replay carts are not game carts.
    # Block save operations and return a special result so callers don't
    # accidentally push save config or attempt save export/import.
    if is_gameshark_header(header):
        return {
            "match_level": "gameshark",
            "is_gameshark": True,
            "matched": None,
            "candidates": [],
            "database": {"path": None, "schema_version": 0, "entry_count": 0},
            "catalog": {"path": None, "entry_count": 0},
            "warnings": [],
            "observed_save_type": observed_save_type,
            "fingerprint": {
                "title": fingerprint["title"],
                "game_id": fingerprint["game_id"],
                "game_id_hex": fingerprint["game_id_hex"],
                "region_code": fingerprint["region_code"],
                "region_name": fingerprint["region_name"],
                "version": fingerprint["version"],
                "rom_size": fingerprint["rom_size"],
                "crc1": fingerprint["crc1"],
                "crc2": fingerprint["crc2"],
                "rom_crc32_ieee": fingerprint["rom_crc32_ieee"],
            },
        }

    if db_path is not None:
        db_path_resolved, schema_version, db_entries = _load_db(db_path)
    else:
        db_path_resolved, schema_version, db_entries = None, 0, []
    if use_catalog_txt:
        catalog_path, catalog_entries = _load_n64txt_catalog(catalog_txt_path)
    else:
        catalog_path, catalog_entries = None, []
    entries = db_entries + catalog_entries

    candidates = []
    for entry in entries:
        candidate = _candidate_from_entry(fingerprint, entry)
        if candidate is not None:
            candidates.append(candidate)

    candidates.sort(
        key=lambda item: (
            item["_rank"],
            item["_rom_crc32_match"],
            item["_has_crc_pair"],
            item["_has_rom_crc32"],
            item["_has_size"],
        ),
        reverse=True,
    )

    finalized_candidates = [_finalize_candidate(item) for item in candidates]
    matched = finalized_candidates[0] if finalized_candidates else None
    match_level = matched["match_level"] if matched else "unknown"
    warnings: list[dict[str, str]] = []

    observed_norm = _normalize_save_hint(observed_save_type)
    matched_hint = _normalize_save_hint(matched.get("save_type_hint") if matched else None)
    if (
        matched is not None
        and matched_hint is not None
        and observed_norm is not None
        and observed_norm not in ("none", "unknown")
        and matched_hint not in ("none", "unknown")
        and matched_hint != observed_norm
    ):
        warnings.append(
            {
                "kind": "save_type_mismatch",
                "message": (
                    f"Matched entry save hint '{matched_hint}' differs from "
                    f"live detected save type '{observed_norm}'."
                ),
                "matched_save_type_hint": matched_hint,
                "observed_save_type": observed_norm,
            }
        )

    result: dict[str, Any] = {
        "match_level": match_level,
        "matched": matched,
        "candidates": finalized_candidates[:5],
        "database": {
            "path": str(db_path_resolved) if db_path_resolved is not None else None,
            "schema_version": schema_version,
            "entry_count": len(db_entries),
        },
        "warnings": warnings,
        "catalog": {
            "path": str(catalog_path) if catalog_path is not None else None,
            "entry_count": len(catalog_entries),
        },
        "observed_save_type": observed_save_type,
        "fingerprint": {
            "title": fingerprint["title"],
            "game_id": fingerprint["game_id"],
            "game_id_hex": fingerprint["game_id_hex"],
            "region_code": fingerprint["region_code"],
            "region_name": fingerprint["region_name"],
            "version": fingerprint["version"],
            "rom_size": fingerprint["rom_size"],
            "crc1": fingerprint["crc1"],
            "crc2": fingerprint["crc2"],
            "rom_crc32_ieee": fingerprint["rom_crc32_ieee"],
        },
    }

    if match_level == "unknown" and emit_unknown_path:
        result["unknown_logged_to"] = _append_unknown_signature(emit_unknown_path, fingerprint)

    return result
