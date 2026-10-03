"""Check 153 exact field/bit bodies and compare the shared operations once."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import compile_and_run, extract_originals


ROOT = Path(__file__).resolve().parents[2]
CANDIDATES = ROOT / "out/function-inventory/next-memory-candidates.json"
MANIFEST = ROOT / "LostOdysseyRecompSemantics/field_bits_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/field_bits_oracle.cpp"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/field_bits.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-field-bits-tests"
FAMILIES = {
    "ReadFieldBits": 99,
    "ReadPointerChainFieldBits": 12,
    "InsertFieldBits": 17,
    "MaskFieldFlags": 9,
    "SetFieldFlags": 16,
}
BODY = re.compile(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{\r?\n.*?\r?\n\}", re.S)


def entries_from_manifest() -> list[dict]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1 or manifest.get("family_counts") != FAMILIES:
        raise ValueError("field-bit family membership changed")
    entries = manifest["entries"]
    if len(entries) != 153 or [e["address"] for e in entries] != sorted(
            {e["address"] for e in entries}):
        raise ValueError("field-bit addresses changed or duplicated")
    candidates = json.loads(CANDIDATES.read_text(encoding="utf-8"))["entries"]
    expected = sorted((e for e in candidates if e["family"] in FAMILIES),
                      key=lambda e: e["address"])
    for entry, candidate in zip(entries, expected, strict=True):
        if entry != {**candidate, "generated_ppc_path": candidate["source"],
                     "line": candidate["source_line"]}:
            raise ValueError(f"field-bit candidate evidence changed: {entry['address']}")
        if entry["instruction_sequence"][-1] != "blr":
            raise ValueError(f"missing terminal blr: {entry['address']}")
        effects = entry["instruction_effects"]
        if len(effects) != len(entry["instruction_sequence"]):
            raise ValueError(f"instruction/effect count changed: {entry['address']}")
        for instruction, effect in zip(entry["instruction_sequence"], effects, strict=True):
            if effect["instruction"] != instruction or len(effect["statements"]) != 1:
                raise ValueError(f"instruction/effect mismatch: {entry['address']}")
        if [effect["statements"][0] for effect in effects[:-1]] != \
                entry["generated_effect_statements"] or effects[-1]["statements"] != ["return;"]:
            raise ValueError(f"generated effects changed: {entry['address']}")
        writes = [dict(instruction_index=i,
                       register=re.match(r"ctx\.(r\d+)\.", effect["statements"][0]).group(1),
                       statement=effect["statements"][0])
                  for i, effect in enumerate(effects[:-1])
                  if effect["statements"][0].startswith("ctx.")]
        if writes != entry["guest_context_side_effects"]["ordered_gpr_writes"] or \
                entry["guest_context_side_effects"]["special_register_effects"]:
            raise ValueError(f"GPR effect contract changed: {entry['address']}")
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
    lines = [
        "enum class FieldBitFamily { ReadFieldBits, ReadPointerChainFieldBits,",
        "    InsertFieldBits, MaskFieldFlags, SetFieldFlags };",
        "struct FieldBitEntry {",
        "    std::uint32_t address;",
        "    void (*original)(PPCContext&, std::uint8_t*);",
        "    FieldBitFamily family;",
        "    std::int32_t offsets[3];",
        "    unsigned width;",
        "    unsigned load_count;",
        "    bool base_r4;",
        "};",
        "static const FieldBitEntry kFieldBitEntries[] = {",
    ]
    for entry in entries:
        params = entry["parameters"]
        loads = params.get("loads")
        offsets = ([load["displacement"] for load in loads] if loads else
                   [params["displacement"]])
        width = loads[-1]["width"] if loads else params["width"]
        count = len(offsets)
        base_r4 = params.get("base_register") == "r4"
        offsets += [0] * (3 - len(offsets))
        lines.append(
            f'    {{0x{entry["address"]}u, &__imp__sub_{entry["address"]}, '
            f'FieldBitFamily::{entry["family"]}, '
            f'{{{offsets[0]}, {offsets[1]}, {offsets[2]}}}, '
            f'{width}, {count}, {str(base_r4).lower()}}},'
        )
    lines.append("};")
    return ("\n".join(lines) + "\n").encode() + ORACLE.read_bytes()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    entries = entries_from_manifest()
    originals = extract_originals(entries, args.ppc_root)
    check_originals(entries, originals)
    result = compile_and_run("field-bits", originals, make_harness(entries),
                             [SOURCE], args.output)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
