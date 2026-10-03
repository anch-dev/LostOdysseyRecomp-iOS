"""Compare all fixed-offset integer accessors with their original PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import compile_and_run, extract_originals


ROOT = Path(__file__).resolve().parents[2]
INVENTORY = ROOT / "out/function-inventory/batch-candidates.json"
MAP = ROOT / "LostOdysseyRecompSemantics/accessor_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/accessor_family_oracle.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-accessor-family-tests"
OPERATION = {
    "lbz": ("getter", "Byte", 8),
    "lhz": ("getter", "Halfword", 16),
    "lwz": ("getter", "Word", 32),
    "stb": ("setter", "Byte", 8),
    "sth": ("setter", "Halfword", 16),
    "stw": ("setter", "Word", 32),
}
INSTRUCTION = re.compile(r"^(lbz|lhz|lwz|stb|sth|stw) r(\d+),(-?\d+)\(r(\d+)\)$")
BODY = re.compile(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{\r?\n.*?\r?\n\}", re.S)


def describe(candidate: dict, kind: str) -> dict:
    address = candidate["address"]
    if not re.fullmatch(r"0x[0-9A-F]{8}", address):
        raise ValueError(f"invalid candidate address: {address}")
    if candidate["already_in_recovery_manifest"]:
        raise ValueError(f"candidate already recovered: {address}")
    instructions = candidate["instruction_sequence"]
    if len(instructions) != 2 or instructions[1] != "blr":
        raise ValueError(f"unexpected instruction sequence: {address}")
    match = INSTRUCTION.fullmatch(instructions[0])
    if match is None:
        raise ValueError(f"unexpected accessor instruction: {address}")
    operation, value_register, displacement, base_register = match.groups()
    semantic, width, bits = OPERATION[operation]
    if semantic != kind or int(base_register) not in (3, 4, 5, 6, 13):
        raise ValueError(f"unsupported accessor registers: {address}")
    if kind == "getter" and int(value_register) != 3:
        raise ValueError(f"getter does not return through r3: {address}")
    if kind == "setter" and int(value_register) not in (3, 4, 5):
        raise ValueError(f"unsupported setter value register: {address}")
    if not -32768 <= int(displacement) <= 32767:
        raise ValueError(f"offset outside PPC displacement: {address}")
    if not re.fullmatch(r"LostOdysseyRecompLib/ppc/ppc_recomp\.\d+\.cpp",
                        candidate["source"]):
        raise ValueError(f"unexpected generated source: {address}")

    if kind == "getter":
        statement = (f"ctx.r3.u64 = PPC_LOAD_U{bits}"
                     f"(ctx.r{base_register}.u32 + {displacement});")
    else:
        statement = (f"PPC_STORE_U{bits}(ctx.r{base_register}.u32 + "
                     f"{displacement}, ctx.r{value_register}.u{bits});")
    if candidate["generated_effect_statements"] != [statement]:
        raise ValueError(f"generated effect changed: {address}")
    return {
        "address": address[2:],
        "kind": kind,
        "width": width,
        "displacement": int(displacement),
        "base_register": int(base_register),
        "value_register": int(value_register) if kind == "setter" else None,
        "generated_ppc_path": candidate["source"],
        "line": candidate["source_line"],
        "instruction_sequence": instructions,
        "effect_statement": statement,
    }


def entries_from_inventory() -> list[dict]:
    candidates = json.loads(INVENTORY.read_text(encoding="utf-8"))["candidates"]
    getters = candidates["single_fixed_offset_integer_getter"]
    setters = candidates["single_fixed_offset_integer_setter"]
    if len(getters) != 217 or len(setters) != 87:
        raise ValueError("fixed-offset candidate membership changed")
    entries = ([describe(item, "getter") for item in getters] +
               [describe(item, "setter") for item in setters])
    entries.sort(key=lambda item: item["address"])
    if len({item["address"] for item in entries}) != 304:
        raise ValueError("duplicate fixed-offset candidate")
    return entries


def check_originals(entries: list[dict], originals: bytes) -> None:
    bodies = BODY.findall(originals)
    if len(bodies) != len(entries):
        raise ValueError("original PPC body count differs from candidate count")
    for entry, address in zip(entries, bodies, strict=True):
        if address.decode("ascii") != entry["address"]:
            raise ValueError(f"original body order differs: {entry['address']}")
    for entry, match in zip(entries, BODY.finditer(originals), strict=True):
        actual = [line.rstrip() for line in match.group().splitlines()]
        expected = [
            f"PPC_FUNC_IMPL(__imp__sub_{entry['address']}) {{".encode(),
            b"\tPPC_FUNC_PROLOGUE();",
            f"\t// {entry['instruction_sequence'][0]}".encode(),
            f"\t{entry['effect_statement']}".encode(),
            b"\t// blr",
            b"\treturn;",
            b"}",
        ]
        if actual != expected:
            raise ValueError(f"original PPC body changed: {entry['address']}")


def make_harness(entries: list[dict]) -> bytes:
    lines = [
        '#include "lo_semantics/accessor_family.h"',
        '#include <cstdint>',
        'using lo::semantic::gpu::IntegerWidth;',
        'struct AccessorEntry {',
        '    std::uint32_t address;',
        '    void (*original)(PPCContext&, std::uint8_t*);',
        '    bool is_getter;',
        '    IntegerWidth width;',
        '    std::int32_t displacement;',
        '    unsigned base_register;',
        '    unsigned value_register;',
        '};',
        'static const AccessorEntry kAccessorEntries[] = {',
    ]
    for entry in entries:
        lines.append(
            f'    {{0x{entry["address"]}u, &__imp__sub_{entry["address"]}, '
            f'{str(entry["kind"] == "getter").lower()}, IntegerWidth::{entry["width"]}, '
            f'{entry["displacement"]}, {entry["base_register"]}, '
            f'{entry["value_register"] or 0}}},'
        )
    lines.append('};')
    return ("\n".join(lines) + "\n").encode() + ORACLE.read_bytes()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--write-map", action="store_true")
    args = parser.parse_args()

    entries = entries_from_inventory()
    originals = extract_originals(entries, args.ppc_root)
    check_originals(entries, originals)
    mapping = {
        "schema_version": 1,
        "kind": "fixed_offset_integer_accessor_families",
        "semantics": {
            "getter": "lo::semantic::gpu::ReadField",
            "setter": "lo::semantic::gpu::WriteField",
        },
        "entries": [
            {key: value for key, value in entry.items()
             if key not in ("instruction_sequence", "effect_statement")}
            for entry in entries
        ],
    }
    if args.write_map:
        MAP.write_text(json.dumps(mapping, indent=2) + "\n", encoding="utf-8")
    elif not MAP.is_file() or json.loads(MAP.read_text(encoding="utf-8")) != mapping:
        raise ValueError("tracked accessor map differs from original PPC candidates")

    result = compile_and_run(
        "accessor-family", originals, make_harness(entries),
        ["LostOdysseyRecompSemantics/src/accessor_family.cpp"], args.output)
    expected = "PASS accessor-family 304 entries 1824 cases 217 getters 87 setters"
    if result["summary"] != expected:
        raise ValueError(f"unexpected oracle summary: {result['summary']}")


if __name__ == "__main__":
    main()
