"""Admit the complete fixed 827CBCA8 PPC resize body and its control flow."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/heap_block_resize_families.json"
CACHE = ROOT / "out/function-inventory/metadata-heap-reallocate-originals.json"
SOURCE = Path("LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp")
ADDRESS = "827CBCA8"
LINE = 1709
COUNT = 348
CALLS = ["827CB778", "823AE108", "827CBA60", "830DA08C",
         "830DA08C", "827CBA60", "82B7BC40"]


def generate(ppc_root: Path | None = None) -> dict:
    candidate = next(x for x in json.loads(CACHE.read_text(encoding="utf-8"))
                     if x["address"] == ADDRESS)
    if (candidate["generated_ppc_path"] != SOURCE.as_posix() or
            candidate["line"] != LINE or candidate["instruction_count"] != COUNT):
        raise ValueError("fixed heap-block source/count changed")
    if ppc_root is None:
        ppc_root = ROOT / "LostOdysseyRecompLib/ppc"
    source = ppc_root / SOURCE.name
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    start = LINE - 1
    if lines[start] != f"PPC_FUNC_IMPL(__imp__sub_{ADDRESS}) {{":
        raise ValueError("heap-block body moved")
    end = start + 1
    while end < len(lines) and lines[end] != "}":
        if lines[end].startswith("PPC_FUNC_IMPL("):
            raise ValueError("heap-block body unterminated")
        end += 1
    body = "\n".join(lines[start:end + 1])
    instructions = re.findall(r"^[ \t]*// (.+)$", body, re.M)
    calls = [x.upper() for x in re.findall(r"// bl 0x([0-9a-f]+)", body)
             if not x.startswith("82b7a6")]
    if body != candidate["translated_body"] or instructions != candidate["instruction_sequence"]:
        raise ValueError("complete heap-block translated source differs from fixed cache")
    if len(instructions) != COUNT or calls != CALLS:
        raise ValueError(f"heap-block instruction/direct-call mismatch: {len(instructions)} {calls}")
    entries = [{
        "address": ADDRESS,
        "source": SOURCE.as_posix(), "line": LINE,
        "instructions": instructions,
        "calls": calls,
        "cfg": {
            "labels": re.findall(r"^loc_([0-9A-F]+):", body, re.M),
            "branches": [x for x in instructions if x.startswith("b") and
                         ("0x" in x or x == "blr")],
        },
        "parameters": {"frame_bytes": 176, "save_first": 22,
            "input_registers": ["r3", "r4", "r5", "r6", "r7"],
            "native_compare": "830DA08C", "free_fill": "FEEE FEEE"},
        "translated_body": body,
        "status": "bounded_readable_library",
        "boundary": "ordinary guest RAM; existing heap segment/coalesce/insert/fill semantics; two native comparisons explicit; generic lower volatile ABI excluded",
    }]
    payload = {"schema": "heap-block-resize-v1", "entries": entries}
    if MANIFEST.exists() and json.loads(MANIFEST.read_text(encoding="utf-8")) != payload:
        raise ValueError("heap-block manifest differs from complete PPC source")
    return payload


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path)
    args = parser.parse_args()
    result = generate(args.ppc_root)
    if not MANIFEST.exists():
        MANIFEST.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
