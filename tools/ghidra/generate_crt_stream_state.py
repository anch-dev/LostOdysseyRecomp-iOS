"""Admit four complete, fixed PPC CRT stream-state bodies."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/crt_stream_state_families.json"
SPECS = (
    ("82B81648", 3681, 22, ["82B7FD78", "82B7FEC0"]),
    ("82B82180", 5567, 9, ["830D9E9C"]),
    ("82B821B0", 5597, 35, ["822CA180"]),
    ("82B85C40", 14758, 34, ["823ACBD0"]),
)


def generate(ppc_root: Path | None = None, *, verify: bool = True) -> dict:
    if ppc_root is None:
        ppc_root = ROOT / "LostOdysseyRecompLib/ppc"
    source = ppc_root / "ppc_recomp.176.cpp"
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc/ppc_recomp.176.cpp"
    lines = source.read_text(encoding="utf-8").splitlines()
    entries = []
    for address, line, count, expected_calls in SPECS:
        start = line - 1
        if lines[start] != f"PPC_FUNC_IMPL(__imp__sub_{address}) {{":
            raise ValueError(f"{address} moved from ppc_recomp.176.cpp:{line}")
        end = start + 1
        while end < len(lines) and lines[end] != "}":
            if lines[end].startswith("PPC_FUNC_IMPL("):
                raise ValueError(f"{address} body terminator missing")
            end += 1
        if end == len(lines):
            raise ValueError(f"{address} body terminator missing")
        body = "\n".join(lines[start:end + 1])
        instructions = re.findall(r"^[ \t]*// (.+)$", body, re.M)
        if len(instructions) != count:
            raise ValueError(f"{address} expected {count} instructions, got {len(instructions)}")
        calls = [target.upper() for target in
                 re.findall(r"// bl 0x([0-9a-f]+)", body)
                 if not target.lower().startswith("82b7a6")]
        if calls != expected_calls:
            raise ValueError(f"{address} direct-call sequence changed: {calls}")
        entries.append({
            "address": address,
            "source": "LostOdysseyRecompLib/ppc/ppc_recomp.176.cpp",
            "line": line,
            "instructions": instructions,
            "calls": calls,
            "cfg": {
                "labels": re.findall(r"^loc_([0-9A-F]+):", body, re.M),
                "branches": [item for item in instructions
                             if re.match(r"b(?:eq|ne|lt|gt|le|ge)?(?: cr6)?(?:,| )", item)],
            },
            "translated_body": body,
            "status": "bounded_readable_library",
            "boundary": "ordinary guest RAM; recovered CRT errno and raw allocator; native critical-section and alternate indirect target explicit",
        })
    payload = {"schema": "crt-stream-state-v1", "entries": entries}
    if verify and (not MANIFEST.exists() or
                   json.loads(MANIFEST.read_text(encoding="utf-8")) != payload):
        raise ValueError("fixed CRT stream manifest differs from complete PPC bodies")
    return payload


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path)
    parser.add_argument("--initialize-manifest", action="store_true")
    args = parser.parse_args()
    if args.initialize_manifest:
        if MANIFEST.exists():
            raise ValueError("manifest already exists; cannot reinitialize baseline")
        MANIFEST.write_text(json.dumps(generate(args.ppc_root, verify=False), indent=2) + "\n",
                            encoding="utf-8")
    else:
        generate(args.ppc_root)


if __name__ == "__main__":
    main()
