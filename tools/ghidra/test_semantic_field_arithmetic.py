"""Compare reviewed field arithmetic with the original PPC for every mapped entry."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re

from semantic_batch import compile_and_run, extract_originals


ROOT = Path(__file__).resolve().parents[2]
CANDIDATES = ROOT / "out/function-inventory/field-arithmetic-candidates.json"
MANIFEST = ROOT / "LostOdysseyRecompSemantics/field_arithmetic_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/field_arithmetic_oracle.cpp"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/field_arithmetic.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-field-arithmetic-tests"
BODY = re.compile(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{\r?\n.*?\r?\n\}", re.S)
FAMILIES = {
    "AddressFromPointerFieldAndIndex": 6,
    "AdjustWordField": 16,
    "ComposeTaggedPointer": 9,
    "ExchangeWordField": 8,
    "ExtractFieldBitsPlusConstant": 1,
    "MultiplyWordFieldByStride": 9,
    "ReadCursorAndAdvance": 3,
    "ReadFieldIndexedPointerMember": 7,
    "ReadIndexedPointerField": 7,
    "ReadLargeOffsetField": 7,
    "RoundWordFieldUnits": 1,
    "ScaleBiasedWordField": 8,
    "SumScaledWordFields": 2,
    "SumWordFields": 5,
}
PARAM_FIELDS = ("field field1 field2 field3 field4 pointer_field inner_pointer_field "
                "base_field index_field tag_field cursor_field member offset_low global_high "
                "global_low delta replacement status bias return_bias region rounding_bias "
                "unit_bias advance shift shift1 shift2").split()
FLAG_FIELDS = {
    "global_high": "HasGlobal", "pointer_field": "HasPointer",
    "inner_pointer_field": "HasInner", "shift1": "HasShift1",
    "field3": "HasField3", "field4": "HasField4",
    "status": "HasStatus", "delta": "HasDelta",
    "replacement": "HasReplacement", "return_bias": "HasReturnBias",
}


def entries_from_manifest() -> list[dict]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1 or manifest.get("family_counts") != FAMILIES:
        raise ValueError("field arithmetic family membership changed")
    entries = manifest["entries"]
    if len(entries) != 89 or [e["address"] for e in entries] != sorted(
            {e["address"] for e in entries}):
        raise ValueError("field arithmetic addresses changed or duplicated")
    if dict(Counter(entry["family"] for entry in entries)) != FAMILIES:
        raise ValueError("field arithmetic family counts changed")
    # The inventory is an ignored discovery cache. Check it when available;
    # the tracked manifest and original source suffice in a clean checkout.
    candidates = (sorted(json.loads(CANDIDATES.read_text(encoding="utf-8"))["entries"],
                         key=lambda e: e["address"]) if CANDIDATES.exists() else None)
    if candidates is not None and len(candidates) != len(entries):
        raise ValueError("discovery cache count changed")
    for i, entry in enumerate(entries):
        if candidates is not None:
            candidate = candidates[i]
            if entry != {**candidate, "status": "bounded_original_ppc_comparison_passed",
                         "generated_ppc_path": candidate["source"],
                         "line": candidate["source_line"]}:
                raise ValueError(f"candidate evidence changed: {entry['address']}")
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
        written = [dict(instruction_index=i,
                        register=re.match(r"ctx\.(r\d+)\.", effect["statements"][0]).group(1),
                        statement=effect["statements"][0])
                   for i, effect in enumerate(effects[:-1])
                   if effect["statements"][0].startswith("ctx.")]
        side_effects = entry["guest_context_side_effects"]
        if written != side_effects["ordered_gpr_writes"] or any(
                side_effects[key] for key in
                ("cr_writes", "xer_writes", "lr_writes", "ctr_writes")):
            raise ValueError(f"guest context contract changed: {entry['address']}")
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
        p = entry["parameters"]
        values = ", ".join(str(p.get(field, 0)) for field in PARAM_FIELDS)
        flags = " | ".join(value for key, value in FLAG_FIELDS.items() if key in p) or "0"
        lines.append(f"    {{0x{entry['address']}u, &__imp__sub_{entry['address']}, "
                     f"Family::{entry['family']}, {{{values}}}, {flags}}},")
    lines.append("};")
    oracle = ORACLE.read_text(encoding="utf-8")
    return oracle.replace("// TEST_ENTRIES", "\n".join(lines)).encode("utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    entries = entries_from_manifest()
    originals = extract_originals(entries, args.ppc_root)
    check_originals(entries, originals)
    result = compile_and_run("field-arithmetic", originals, make_harness(entries),
                             [SOURCE], args.output)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
