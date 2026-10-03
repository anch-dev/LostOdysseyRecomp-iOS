"""Compare recovered memory-write operations with their original PPC bodies."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re

from semantic_batch import compile_and_run, extract_originals


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/memory_write_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/memory_writes_oracle.cpp"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/memory_writes.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-memory-write-tests"
BODY = re.compile(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{\r?\n.*?\r?\n\}", re.S)


def entries_from_manifest() -> list[dict]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    entries = manifest["entries"]
    if manifest.get("schema_version") != 1 or manifest.get("kind") != "memory_write_semantics":
        raise ValueError("memory-write manifest schema changed")
    if len(entries) != 28 or [e["address"] for e in entries] != sorted(
            {e["address"] for e in entries}):
        raise ValueError("memory-write address membership changed")
    if dict(Counter(e["family"] for e in entries)) != manifest["family_counts"]:
        raise ValueError("memory-write family counts changed")
    for entry in entries:
        effects = entry["instruction_effects"]
        sequence = entry["instruction_sequence"]
        if len(effects) != len(sequence) or sequence[-1] != "blr":
            raise ValueError(f"instruction sequence changed: {entry['address']}")
        if any(effect["instruction"] != instruction or len(effect["statements"]) != 1
               for instruction, effect in zip(sequence, effects, strict=True)):
            raise ValueError(f"instruction effects changed: {entry['address']}")
        statements = [effect["statements"][0] for effect in effects]
        if entry["generated_statements"] != ["PPC_FUNC_PROLOGUE();", *statements] or \
                statements[-1] != "return;":
            raise ValueError(f"generated effects changed: {entry['address']}")
        if sum("PPC_STORE_" in statement for statement in statements) < 2:
            raise ValueError(f"expected multiple writes: {entry['address']}")
    return entries


def check_originals(entries: list[dict], originals: bytes) -> None:
    bodies = list(BODY.finditer(originals))
    if len(bodies) != len(entries):
        raise ValueError("original PPC body count changed")
    for entry, match in zip(entries, bodies, strict=True):
        if match.group(1).decode("ascii") != entry["address"]:
            raise ValueError(f"original body order changed: {entry['address']}")
        expected = [f"PPC_FUNC_IMPL(__imp__sub_{entry['address']}) {{".encode(),
                    b"\tPPC_FUNC_PROLOGUE();"]
        for effect in entry["instruction_effects"]:
            expected.extend((f"\t// {effect['instruction']}".encode(),
                             f"\t{effect['statements'][0]}".encode()))
        expected.append(b"}")
        if [line.rstrip() for line in match.group().splitlines()] != expected:
            raise ValueError(f"original generated statements changed: {entry['address']}")


def make_harness(entries: list[dict]) -> bytes:
    lines = ["static const TestEntry kTestEntries[] = {"]
    for entry in entries:
        address = entry["address"]
        lines.append(f"    {{0x{address}u, &__imp__sub_{address}}},")
    lines.append("};")
    return ORACLE.read_text(encoding="utf-8").replace(
        "// TEST_ENTRIES", "\n".join(lines)).encode("utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    entries = entries_from_manifest()
    originals = extract_originals(entries, args.ppc_root)
    check_originals(entries, originals)
    result = compile_and_run("memory-writes", originals, make_harness(entries),
                             [SOURCE], args.output)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
