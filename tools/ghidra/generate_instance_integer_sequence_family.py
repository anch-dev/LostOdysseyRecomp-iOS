"""Pin fourteen reviewed integer-only initializer bodies and their parameters."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, write_if_changed


ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
CACHE = ROOT / "out/function-inventory"
MANIFEST = SEMANTICS / "instance_integer_sequence_families.json"
SOURCE = SEMANTICS / "src/instance_integer_sequence_family.cpp"
BEGIN = "    // BEGIN GENERATED INSTANCE INTEGER SEQUENCE PARAMETERS\n"
END = "    // END GENERATED INSTANCE INTEGER SEQUENCE PARAMETERS\n"

# Layout, vtable, four additional ordered words, doubleword and instruction
# count. Constants are reviewed from the cached exact PPC store sources.
APPROVED = {
    "8240AF30": ("TextBufferFactory", 0x821911C0, 0, 0, 0, 0, 0, 16),
    "8245C678": ("Commandlet", 0x821A75A0, 0, 0, 0, 0, 0, 12),
    "8245C710": ("Commandlet", 0x821A7490, 0, 0, 0, 0, 0, 12),
    "824772F8": ("XeAudioDevice", 0x82005508, 0x82062CC0, 0x82000D04, 0, 0, 0, 53),
    "824C1328": ("ShaderCache", 0x821AD350, 0, 0, 0, 0, 0, 26),
    "825E2A98": ("GameViewportClient", 0x82003D38, 0x821DE5F0, 0x82062CC0,
                 0x82002748, 0x82000D20, 0x04023BC200003362, 27),
    "825F2860": ("Exporter", 0x821E0160, 0, 0, 0, 0, 0, 9),
    "825F4450": ("Factory", 0x82190B30, 0, 0, 0, 0, 0, 16),
    "826980D0": ("ShadowMap1D", 0x821FC2E8, 0x8218D6F8, 0x821CB43C,
                 0x821FC3F0, 0x82203160, 0, 31),
    "826A0D28": ("DrawString", 0x821FDED8, 0x821590C4, 0x821FE010, 0, 0, 0, 21),
    "826A0EA0": ("DrawStringLate", 0x821FE020, 0x821590C4, 0x821FE010, 0, 0, 0, 21),
    "826A0FB8": ("DrawStringLate", 0x821FE158, 0x821590C4, 0x821FE010, 0, 0, 0, 21),
    "826E0918": ("StaticMesh", 0x822031F8, 0, 0, 0, 0, 0, 30),
    "826F0C38": ("AudioDevice", 0x821A8150, 0x82062CC0, 0x821A828C, 0, 0, 0, 32),
}


def constant_stores(instructions: list[str]) -> list[tuple[str, int]]:
    """Read only literal arithmetic needed to verify reviewed table values."""
    registers: dict[str, int | None] = {}
    stores = []
    for instruction in instructions:
        if match := re.fullmatch(r"lis (r\d+),(-?\d+)", instruction):
            registers[match[1]] = int(match[2]) << 16
        elif match := re.fullmatch(r"li (r\d+),(-?\d+)", instruction):
            registers[match[1]] = int(match[2])
        elif match := re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", instruction):
            base = registers.get(match[2])
            registers[match[1]] = base + int(match[3]) if base is not None else None
        elif match := re.fullmatch(r"ori (r\d+),(r\d+),(\d+)", instruction):
            base = registers.get(match[2])
            registers[match[1]] = base | int(match[3]) if base is not None else None
        elif match := re.fullmatch(r"rldimi (r\d+),(r\d+),32,0", instruction):
            low, high = registers.get(match[1]), registers.get(match[2])
            registers[match[1]] = ((low & 0xffffffff) | ((high & 0xffffffff) << 32)
                                   if low is not None and high is not None else None)
        elif match := re.fullmatch(r"(lwz|ld) (r\d+),-?\d+\(r\d+\)", instruction):
            registers[match[2]] = None
        elif match := re.fullmatch(r"(stw|stb|std) (r\d+),-?\d+\(r\d+\)", instruction):
            value = registers.get(match[2])
            if value is not None:
                stores.append((match[1], value & (0xffffffffffffffff if match[1] == "std"
                                                  else 0xffffffff)))
    return stores


def expected_manifest() -> dict:
    candidates = json.loads((CACHE / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    bodies = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    indexed = {entry["address"]: entry for entry in candidates["entries"]}
    if len(indexed) != len(bodies) or len(bodies) != 786 or len(APPROVED) != 14:
        raise ValueError("cached 786-body instance inventory changed")
    prior = {entry["address"] for path in SEMANTICS.glob("*_families.json")
             if path != MANIFEST
             for entry in json.loads(path.read_text(encoding="utf-8"))["entries"]}
    if overlap := set(APPROVED) & prior:
        raise ValueError(f"integer sequence overlaps existing mapping: {sorted(overlap)}")
    rows = []
    for address, spec in sorted(APPROVED.items()):
        candidate = indexed[address]
        body = bodies[address]
        instructions = candidate["instructions"]
        if len(instructions) != spec[7] or candidate["direct_calls"] or \
                instructions[:2] != ["cmplwi cr6,r3,0", "beqlr cr6"] and \
                address != "824772F8" or \
                any(re.search(r"\bf\d+\b", item) for item in instructions) or \
                any(item.startswith("bl ") or re.fullmatch(r"b 0x[0-9a-fA-F]+", item)
                    for item in instructions):
            raise ValueError(f"unreviewed integer body shape/dependency: {address}")
        if address == "824772F8" and instructions[:3] != [
                "stw r3,20(r1)", "cmplwi cr6,r3,0", "beqlr cr6"]:
            raise ValueError("XeAudio pre-null stack write changed")
        comments = [line[3:] for line in body if line.startswith("// ")]
        if comments != instructions or body[0] != \
                f"PPC_FUNC_IMPL(__imp__sub_{address}) {{" or body[-1] != "}":
            raise ValueError(f"complete cached body/comments changed: {address}")
        literal_stores = constant_stores(instructions)
        for value in (value for value in spec[1:7] if value):
            width = "std" if value > 0xffffffff else "stw"
            if (width, value) not in literal_stores:
                raise ValueError(f"literal parameter absent from stores: {address} {value:x}")
        rows.append({
            "address": address, "layout": spec[0],
            "vtable": f"0x{spec[1]:08X}",
            "extra_words": [f"0x{value:08X}" for value in spec[2:6]],
            "doubleword": f"0x{spec[6]:016X}",
            "source": candidate["generated_ppc_path"],
            "source_line": candidate["line"],
            "registered_types": candidate["types"],
            "cfg_branches": candidate["cfg_branches"],
            "instructions": instructions,
            "original_body": body,
        })
    return {"schema_version": 1, "family": "instance_integer_sequence",
            "entry_count": 14, "source":
            "out/function-inventory/registered-instance-candidates.json",
            "scope": "Full cached translated bodies, CFG, instructions and literal parameters."
                     " Ordinary memory/full r3; volatile GPR/CR and U64 fault/MMIO width excluded.",
            "entries": rows}


def source_with_table() -> str:
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("instance integer source markers changed")
    prefix, remainder = source.split(BEGIN, 1)
    _, suffix = remainder.split(END, 1)
    lines = []
    for address, values in sorted(APPROVED.items()):
        layout, vtable, *rest = values
        words = rest[:4]
        doubleword = rest[4]
        formatted = [f"0x{value:08x}u" if value else "0u"
                     for value in (vtable, *words)]
        wide = f"0x{doubleword:016x}ull" if doubleword else "0ull"
        lines.append(f"    {{0x{address.lower()}u, Layout::{layout}, " +
                     ", ".join((*formatted, wide)) + "},\n")
    return prefix + BEGIN + "".join(lines) + END + suffix


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = json.dumps(expected_manifest(), indent=2) + "\n"
    source = source_with_table()
    if args.check:
        if not MANIFEST.exists() or MANIFEST.read_text(encoding="utf-8") != expected or \
                SOURCE.read_text(encoding="utf-8") != source:
            raise ValueError("integer sequence manifest/source differ from cached PPC")
    else:
        write_if_changed(MANIFEST, expected)
        write_if_changed(SOURCE, source)
    print("validated 14 exact integer instance initializer bodies")


if __name__ == "__main__":
    main()
