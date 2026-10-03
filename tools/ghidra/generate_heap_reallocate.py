"""Admit the complete fixed 827CCF80 PPC reallocation body."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/heap_reallocate_families.json"
CACHE = ROOT / "out/function-inventory/metadata-descriptor-allocation-lower-originals.json"
SOURCE = Path("LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp")
ADDRESS, LINE, COUNT = "827CCF80", 4384, 519
DIRECT = ["827CBA60", "827CBCA8", "823ACCB0", "82B7C470", "82B7BC40",
          "823ADE28", "827CD7BC"]
NATIVE = ["KeGetCurrentProcessType", "KeBugCheckEx",
          "RtlEnterCriticalSection", "NtFreeVirtualMemory",
          "RtlCompareMemoryUlong", "RtlRaiseException"]


def generate(ppc_root: Path | None = None) -> dict:
    cached = next(item for item in json.loads(CACHE.read_text(encoding="utf-8"))
                  if item["address"] == ADDRESS)
    if cached["source"] != SOURCE.as_posix() or cached["line"] != LINE or len(cached["instructions"]) != COUNT:
        raise ValueError("heap reallocate fixed source/count changed")
    if ppc_root is None:
        ppc_root = ROOT / "LostOdysseyRecompLib/ppc"
    source = ppc_root / SOURCE.name
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    start = LINE - 1
    if lines[start] != f"PPC_FUNC_IMPL(__imp__sub_{ADDRESS}) {{":
        raise ValueError("heap reallocate body moved")
    end = next(i for i in range(start + 1, len(lines)) if lines[i] == "}")
    body = "\n".join(lines[start:end + 1])
    instructions = [item.rstrip() for item in re.findall(r"^[ \t]*// (.+)$", body, re.M)]
    calls = [item.upper() for item in re.findall(r"// bl 0x([0-9a-f]+)", body)
             if not item.startswith("82b7a")]
    direct = [item for item in calls if item in DIRECT]
    imports = re.findall(r"\b__imp__([A-Za-z0-9_]+)\(ctx, base\);", body)
    if body != cached["translated_body"] or instructions != cached["instructions"]:
        raise ValueError("complete heap reallocate source differs from fixed cache")
    if len(instructions) != COUNT or sorted(set(direct)) != sorted(DIRECT) or imports != NATIVE:
        raise ValueError(f"heap reallocate call/instruction drift: {len(instructions)} {direct} {imports}")
    entry = {"address": ADDRESS, "source": SOURCE.as_posix(), "line": LINE,
        "instructions": instructions, "direct_calls": direct,
        "native_calls": imports,
        "cfg": {"labels": re.findall(r"^loc_([0-9A-F]+):", body, re.M),
            "branches": [line for line in instructions if line.startswith("b") and
                         ("0x" in line or line == "blr")]},
        "translated_body": body, "status": "bounded_readable_library",
        "boundary": "Ordinary guest RAM, selected parent PPC state and owned save19 frame; all direct PPC calls use recovered C++ models. Native imports are explicit. Generic lower ABI, faults, MMIO, concurrent mutation and game runtime are unverified."}
    result = {"schema": "heap-reallocate-v1", "entries": [entry]}
    if MANIFEST.exists() and json.loads(MANIFEST.read_text(encoding="utf-8")) != result:
        raise ValueError("heap reallocate manifest differs from complete PPC source")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=None)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(args.ppc_root), indent=2) + "\n"
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")


if __name__ == "__main__":
    main()
