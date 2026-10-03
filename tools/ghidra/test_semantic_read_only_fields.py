"""Compare all reviewed read-only field operations with their original PPC bodies."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re

from semantic_batch import compile_and_run, extract_originals


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/read_only_field_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/read_only_fields_oracle.cpp"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/read_only_fields.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-read-only-field-tests"
BODY = re.compile(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{\r?\n.*?\r?\n\}", re.S)
COUNTS = {
    "ComputeAddressOrValue": 5,
    "ReadBitField": 9,
    "ReadBooleanOrMask": 14,
    "ReadTableField": 11,
}


def entries_from_manifest() -> list[dict]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    entries = manifest["entries"]
    if manifest.get("schema_version") != 1 or manifest.get("kind") != "strict_read_only_field_semantics":
        raise ValueError("read-only field schema changed")
    if manifest.get("family_counts") != COUNTS or len(entries) != 39:
        raise ValueError("read-only field membership changed")
    if [e["address"] for e in entries] != sorted({e["address"] for e in entries}):
        raise ValueError("read-only field addresses changed or duplicated")
    if dict(Counter(e["family"] for e in entries)) != COUNTS:
        raise ValueError("read-only field family counts changed")
    for entry in entries:
        address = entry["address"]
        instructions = entry["instruction_sequence"]
        effects = entry["instruction_effects"]
        statements = entry["generated_statements"]
        if instructions[-1] != "blr" or any(x.startswith(("st", "bl ")) for x in instructions[:-1]):
            raise ValueError(f"read-only instruction contract changed: {address}")
        if len(effects) != len(instructions) or statements[0] != "PPC_FUNC_PROLOGUE();":
            raise ValueError(f"statement count changed: {address}")
        if [x["instruction"] for x in effects] != instructions or any(
                len(x["statements"]) != 1 for x in effects):
            raise ValueError(f"instruction effect changed: {address}")
        if statements[1:] != [x["statements"][0] for x in effects]:
            raise ValueError(f"generated effect changed: {address}")
        writes = [re.match(r"ctx\.(r\d+)\.", x).group(1) for x in statements
                  if re.match(r"ctx\.(r\d+)\.", x)]
        if writes != entry["gpr_writes"]:
            raise ValueError(f"register effects changed: {address}")
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
    rows = ["static const TestEntry kTestEntries[] = {"]
    rows += [f"    {{0x{e['address']}u, &__imp__sub_{e['address']}}},"
             for e in entries]
    rows.append("};")
    return ORACLE.read_text(encoding="utf-8").replace(
        "// TEST_ENTRIES", "\n".join(rows)).encode("utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    entries = entries_from_manifest()
    originals = extract_originals(entries, args.ppc_root)
    check_originals(entries, originals)
    compile_and_run("read-only-fields", originals, make_harness(entries),
                    [SOURCE], args.output)


if __name__ == "__main__":
    main()
