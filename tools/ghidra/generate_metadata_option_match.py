"""Admit three fixed, complete PPC UTF-16 option-matching bodies."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_option_match_families.json"
SPECS = (
    ("82296858", "ppc_recomp.0.cpp", 15551, 55, ["82B7FD78", "82B7FEC0"]),
    ("82297390", "ppc_recomp.0.cpp", 17423, 69, ["82296830", "82296858"]),
    ("8247C0C0", "ppc_recomp.23.cpp", 25874, 46,
     ["82297390", "82296830", "82297390"]),
)


def generate(ppc_root: Path | None = None, *, verify: bool = True) -> dict:
    if ppc_root is None:
        ppc_root = ROOT / "LostOdysseyRecompLib/ppc"
    entries = []
    for address, filename, line, count, direct_calls in SPECS:
        source = ppc_root / filename
        if not source.exists():
            source = Path.home() / "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc" / filename
        lines = source.read_text(encoding="utf-8").splitlines()
        start = line - 1
        if lines[start] != f"PPC_FUNC_IMPL(__imp__sub_{address}) {{":
            raise ValueError(f"{address} moved from fixed {filename}:{line}")
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
            raise ValueError(f"{address}: {len(instructions)} != {count} instructions")
        calls = [target.upper() for target in
                 re.findall(r"// bl 0x([0-9a-f]+)", body)
                 if not target.lower().startswith("82b7a6")]
        if calls != direct_calls:
            raise ValueError(f"{address} direct-call sequence changed: {calls}")
        entries.append({
            "address": address,
            "source": f"LostOdysseyRecompLib/ppc/{filename}",
            "line": line,
            "instructions": instructions,
            "calls": calls,
            "cfg": {
                "labels": re.findall(r"^loc_([0-9A-F]+):", body, re.M),
                "branches": [instruction for instruction in instructions
                             if re.match(r"b(?:eq|ne|lt|gt|le|ge)?(?: cr6)?,?", instruction)],
            },
            "translated_body": body,
            "status": "bounded_readable_library",
            "boundary": "ordinary guest RAM, recovered UTF16 length and CRT errno/invalid helpers; nested generic volatile ABI excluded",
        })
    payload = {"schema": "metadata-option-match-v1", "entries": entries}
    if verify:
        if not MANIFEST.exists() or json.loads(MANIFEST.read_text(encoding="utf-8")) != payload:
            raise ValueError("fixed metadata option manifest differs from complete source bodies")
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
