"""Admit seven exact loaded-single instance extension bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, write_if_changed


ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
CACHE = ROOT / "out/function-inventory"
MANIFEST = SEMANTICS / "instance_fp_extension_families.json"
SOURCE = SEMANTICS / "src/instance_fp_extension_family.cpp"
BEGIN = "    // BEGIN GENERATED INSTANCE FP EXTENSION PARAMETERS\n"
END = "    // END GENERATED INSTANCE FP EXTENSION PARAMETERS\n"
APPROVED = {
    "82592690": ("SkeletalMesh", 0x8200CF08, 0, 0, 56),
    "825AECF0": ("LineBatch", 0x8200C5C0, 0x821BAD58, 0x821DA5D0, 43),
    "825E09B8": ("Primitive", 0x8207354C, 0, 0, 32),
    "826BDC18": ("Model", 0x82200AE8, 0, 0, 41),
    "826C00E8": ("Terrain", 0x822013B0, 0, 0, 61),
    "8270DD80": ("Brush", 0x8200B188, 0, 0, 35),
    "82730310": ("ForceFeedback", 0x82003F88, 0, 0, 12),
}


def stores(first: int, last: int, register: str = "r11", base: str = "r3") -> list[str]:
    return [f"stw {register},{offset}({base})" for offset in range(first, last + 1, 4)]


PREFIX = ["stw r11,96(r3)", *stores(368, 376), "std r11,476(r3)",
          "stfs f13,496(r3)"]
TAIL = [*(f"stfs f0,{offset}(r3)" for offset in (504, 508, 512)),
        "stfs f13,516(r3)",
        *(f"stfs f0,{offset}(r3)" for offset in (520, 524, 528, 532)),
        "stfs f13,536(r3)",
        *(f"stfs f0,{offset}(r3)" for offset in (540, 544, 548, 552)),
        "stfs f13,556(r3)"]
LFS13 = "lfs f13,-27252(r11)"
LFS0 = "lfs f0,3664(r11)"

EVENTS = {
    "SkeletalMesh": [LFS13, LFS0, *PREFIX, "stfs f0,500(r3)",
                     "stw r10,0(r3)", *TAIL,
                     *stores(652, 660), *stores(688, 696),
                     *stores(708, 740), *stores(948, 980)],
    "LineBatch": [LFS13, LFS0, *PREFIX, "stfs f0,500(r3)",
                  "stw r10,624(r3)", TAIL[0], "stw r11,628(r3)",
                  *TAIL[1:], "stw r9,0(r3)", "stw r8,624(r3)",
                  *stores(632, 640), "stfs f0,644(r3)"],
    "Primitive": [LFS13, "stw r10,0(r3)", LFS0, *PREFIX,
                  "stfs f0,500(r3)", *TAIL],
    "Model": [LFS13, LFS0, *PREFIX, "stfs f0,500(r3)",
              "stw r10,0(r3)", *TAIL, *stores(636, 668)],
    "Terrain": [LFS13, LFS0, *PREFIX, "stw r10,0(r3)",
                "stfs f0,500(r3)", *TAIL,
                *stores(624, 644), *stores(676, 712),
                *stores(0, 20, base="r10"), *stores(764, 772),
                *stores(784, 792)],
    "Brush": [LFS13, LFS0, *PREFIX, "stfs f0,500(r3)",
              "stw r10,0(r3)", *TAIL, *stores(684, 692)],
    "ForceFeedback": ["lwz r9,60(r3)", "lfs f0,-27252(r10)",
                      "stfs f0,76(r3)", "stw r11,0(r3)",
                      "stw r10,60(r3)"],
}


def memory_events(instructions: list[str]) -> list[str]:
    return [item for item in instructions if re.match(r"(?:lwz|lfs|stw|std|stfs) ", item)]


def literal_stores(instructions: list[str]) -> list[tuple[str, int, int]]:
    registers: dict[str, int | None] = {}
    found = []
    for item in instructions:
        if match := re.fullmatch(r"lis (r\d+),(-?\d+)", item):
            registers[match[1]] = int(match[2]) << 16
        elif match := re.fullmatch(r"li (r\d+),(-?\d+)", item):
            registers[match[1]] = int(match[2])
        elif match := re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", item):
            base = registers.get(match[2])
            registers[match[1]] = base + int(match[3]) if base is not None else None
        elif match := re.fullmatch(r"lwz (r\d+),-?\d+\(r\d+\)", item):
            registers[match[1]] = None
        elif match := re.fullmatch(r"stw (r\d+),(\d+)\(r3\)", item):
            value = registers.get(match[1])
            if value is not None:
                found.append((match[1], int(match[2]), value & 0xffffffff))
    return found


def expected_manifest() -> dict:
    candidates = json.loads((CACHE / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    bodies = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    by_address = {entry["address"]: entry for entry in candidates["entries"]}
    if len(by_address) != len(bodies) or len(bodies) != 786 or len(APPROVED) != 7:
        raise ValueError("cached instance inventory changed")
    previous = {entry["address"] for path in SEMANTICS.glob("*_families.json")
                if path != MANIFEST
                for entry in json.loads(path.read_text(encoding="utf-8"))["entries"]}
    if overlap := set(APPROVED) & previous:
        raise ValueError(f"FP extension overlaps existing mapping: {sorted(overlap)}")
    entries = []
    for address, (layout, vtable, first, final, count) in sorted(APPROVED.items()):
        candidate = by_address[address]
        instructions = candidate["instructions"]
        body = bodies[address]
        if len(instructions) != count or candidate["cfg_branches"] or \
                candidate["direct_calls"] or \
                instructions[:2] != ["cmplwi cr6,r3,0", "beqlr cr6"] or \
                instructions[-1] != "blr" or \
                memory_events(instructions) != EVENTS[layout] or \
                any(re.match(r"f(?:add|sub|mul|div|madd|neg|abs|cmp)", item)
                    for item in instructions):
            raise ValueError(f"FP extension instruction/order/CFG changed: {address}")
        comments = [line[3:] for line in body if line.startswith("// ")]
        if comments != instructions or body[0] != \
                f"PPC_FUNC_IMPL(__imp__sub_{address}) {{" or body[-1] != "}":
            raise ValueError(f"full translated FP body changed: {address}")
        literal = literal_stores(instructions)
        for offset, value in ((0, vtable), (624, first), (624, final)):
            if value and not any(place == offset and word == value
                                 for _, place, word in literal):
                raise ValueError(f"FP extension literal parameter changed: {address}")
        entries.append({
            "address": address, "layout": layout,
            "vtable": f"0x{vtable:08X}",
            "first_word": f"0x{first:08X}",
            "final_word": f"0x{final:08X}",
            "source": candidate["generated_ppc_path"],
            "source_line": candidate["line"],
            "registered_types": candidate["types"],
            "instructions": instructions,
            "original_body": body,
        })
    return {"schema_version": 1, "family": "instance_fp_extension",
            "entry_count": 7, "source":
            "out/function-inventory/registered-instance-candidates.json",
            "float_sources": ["0x8218958C", "0x82000E50"],
            "limitations": [
                "LoadedSingle models lfs/stfs without arithmetic; generated-C++ signaling-NaN FPR behavior can differ.",
                "Original std zero store is modeled as two ordinary-memory U32 writes; fault/MMIO width is excluded.",
            ], "entries": entries}


def source_with_table() -> str:
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("FP extension parameter markers changed")
    prefix, remainder = source.split(BEGIN, 1)
    _, suffix = remainder.split(END, 1)
    rows = "".join(
        f"    {{0x{address.lower()}u, Layout::{layout}, 0x{vtable:08x}u, "
        f"{f'0x{first:08x}u' if first else '0u'}, "
        f"{f'0x{final:08x}u' if final else '0u'}}},\n"
        for address, (layout, vtable, first, final, _) in sorted(APPROVED.items()))
    return prefix + BEGIN + rows + END + suffix


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = json.dumps(expected_manifest(), indent=2) + "\n"
    source = source_with_table()
    if args.check:
        if not MANIFEST.exists() or MANIFEST.read_text(encoding="utf-8") != expected or \
                SOURCE.read_text(encoding="utf-8") != source:
            raise ValueError("FP extension source/manifest differ from cached PPC")
    else:
        write_if_changed(MANIFEST, expected)
        write_if_changed(SOURCE, source)
    print("validated seven exact loaded-single instance extensions")


if __name__ == "__main__":
    main()
