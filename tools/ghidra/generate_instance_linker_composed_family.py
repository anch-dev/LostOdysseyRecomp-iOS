"""Admit three exact cached Linker/archive bodies and emit their parameters."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_composed_initializer_family import expected_body
from generate_instance_tail_initializer_family import generate as lower_generate
from generate_instance_vtable_family import raw_bodies, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "out/function-inventory"
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "instance_linker_composed_families.json"
SOURCE = SEMANTICS / "src/instance_linker_composed_family.cpp"
BEGIN = "    // BEGIN GENERATED INSTANCE LINKER WRAPPERS\n"
END = "    // END GENERATED INSTANCE LINKER WRAPPERS\n"

# These fixed sequences admit only the three reviewed functions. Translation
# validation below checks every cached C++ statement and branch label too.
INSTRUCTIONS = {
    "823F34B8": """lis r11,-31965
lwz r10,23204(r11)
li r11,0
stw r10,4(r3)
lis r10,-31965
lwz r9,23200(r10)
li r10,1
stw r9,8(r3)
lis r9,-31965
lwz r9,23212(r9)
stw r11,16(r3)
stw r11,20(r3)
stw r11,24(r3)
stw r11,28(r3)
stw r9,12(r3)
li r9,-1
stw r11,44(r3)
stw r11,48(r3)
stw r10,32(r3)
stw r10,36(r3)
stw r10,40(r3)
stw r11,52(r3)
stw r11,56(r3)
stw r11,60(r3)
stw r11,64(r3)
stw r11,68(r3)
stw r11,72(r3)
stw r11,76(r3)
stw r10,80(r3)
stw r11,84(r3)
stw r11,88(r3)
stw r11,92(r3)
stw r9,96(r3)
stw r11,100(r3)
stw r11,104(r3)
blr""".splitlines(),
    "824190F8": """mflr r12
stw r12,-8(r1)
std r31,-16(r1)
stwu r1,-96(r1)
mr r31,r3
cmplwi cr6,r31,0
beq cr6,0x82419160
bl 0x82419178
lis r11,-32231
addi r3,r31,240
addi r11,r11,-26264
stw r11,0(r3)
bl 0x823f34b8
lis r11,-32231
addi r10,r11,7880
lis r11,-32231
addi r9,r11,8144
li r11,0
stw r10,0(r31)
stw r9,240(r31)
stw r11,356(r31)
stw r11,360(r31)
stw r11,364(r31)
stw r11,368(r31)
stw r11,372(r31)
stw r11,376(r31)
addi r1,r1,96
lwz r12,-8(r1)
mtlr r12
ld r31,-16(r1)
blr""".splitlines(),
    "82419230": """mflr r12
stw r12,-8(r1)
std r31,-16(r1)
stwu r1,-96(r1)
mr r31,r3
cmplwi cr6,r31,0
beq cr6,0x8241928c
bl 0x82419178
lis r11,-32231
addi r3,r31,240
addi r11,r11,-26264
stw r11,0(r3)
bl 0x823f34b8
lis r11,-32231
addi r10,r11,7608
lis r11,-32256
addi r9,r11,11568
li r11,0
stw r10,0(r31)
stw r9,240(r31)
stw r11,1384(r31)
stw r11,1388(r31)
stw r11,1392(r31)
addi r1,r1,96
lwz r12,-8(r1)
mtlr r12
ld r31,-16(r1)
blr""".splitlines(),
}

WRAPPERS = {
    "824190F8": (0x82419118, 0x8241912C, 0x82191EC8, 0x82191FD0, 376),
    "82419230": (0x82419250, 0x82419264, 0x82191DB8, 0x82002D30, 1392),
}


def cached_base() -> list[str]:
    lines = (CACHE / "next-instance-hubs.cpp").read_text(
        encoding="utf-8").splitlines()
    if lines[0] != "PPC_FUNC_IMPL(__imp__sub_823F34B8) {":
        raise ValueError("823F34B8 cache boundary changed")
    end = lines.index("}")
    return [line.strip() for line in lines[:end + 1]]


def lower_proof() -> list[dict]:
    manifest, source = lower_generate()
    if json.loads((SEMANTICS / "instance_tail_initializer_families.json")
                  .read_text(encoding="utf-8")) != manifest or \
            (SEMANTICS / "src/instance_tail_initializer_family.cpp")\
                    .read_text(encoding="utf-8") != source:
        raise ValueError("82419178 lower implementation/body changed")
    lower = [entry for entry in manifest["entries"] if
             entry["address"] == "82419178"]
    if len(lower) != 1 or lower[0]["direct_calls"] != ["82B7BC40"]:
        raise ValueError("82419178 lower dependency changed")
    recovery = json.loads((SEMANTICS / "recovery.json").read_text(
        encoding="utf-8"))
    fill = [entry for entry in recovery["functions"] if
            entry["address"] == "82B7BC40"]
    if len(fill) != 1 or not all(fill[0]["stages"][stage] for stage in
                                 ("readable_implementation",
                                  "differential_validation")):
        raise ValueError("82B7BC40 fill proof changed")
    return [{"address": "82419178", "implementation":
             "lo::semantic::gpu::instance_tail_initializer_family::Apply",
             "bounded_oracle":
             "tools/ghidra/test_semantic_instance_tail_initializer_family.py"},
            {"address": "82B7BC40", "implementation":
             "lo::semantic::gpu::FillGuestMemory",
             "bounded_oracle": "tools/ghidra/test_semantic_memory_fill.py"}]


def generate() -> tuple[dict, str]:
    candidates = json.loads((CACHE / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    mapped = {entry["address"]: entry for entry in candidates["entries"]}
    originals = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    bodies = {"823F34B8": cached_base(),
              **{address: originals[address] for address in WRAPPERS}}
    entries = []
    for address in sorted(INSTRUCTIONS):
        instructions = INSTRUCTIONS[address]
        body = bodies[address]
        if body != expected_body(address, instructions):
            raise ValueError(f"{address}: complete translated body/CFG changed")
        if address in WRAPPERS:
            candidate = mapped[address]
            if candidate["instructions"] != instructions or \
                    candidate["instruction_count"] != len(instructions) or \
                    candidate["direct_calls"] != ["82419178", "823F34B8"]:
                raise ValueError(f"{address}: wrapper instruction/call changed")
            source = candidate["generated_ppc_path"]
            line = candidate["line"]
        else:
            source = "LostOdysseyRecompLib/ppc/ppc_recomp.14.cpp"
            line = 30937
        branches = [part for part in instructions if
                    part.startswith(("beq cr6,", "bne cr6,", "b 0x"))]
        calls = [part.split("0x", 1)[1].upper() for part in instructions
                 if part.startswith("bl 0x")]
        entries.append({"address": address,
                        "kind": "archive_fields" if address == "823F34B8"
                                else "frame_wrapper",
                        "source": source, "source_line": line,
                        "instruction_sequence": instructions,
                        "cfg_branches": branches, "direct_calls": calls,
                        "body": body})
    rows = [f"    {{0x{address.lower()}u, " +
            ", ".join(f"0x{value:08x}u" if index < 4 else f"{value}u"
                      for index, value in enumerate(values)) + "},\n"
            for address, values in sorted(WRAPPERS.items())]
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("Linker wrapper marker changed")
    before, tail = source.split(BEGIN)
    _, after = tail.split(END)
    source = before + BEGIN + "".join(rows) + END + after
    manifest = {"schema_version": 1, "entry_count": 3,
                "reused_lower": lower_proof(), "entries": entries,
                "bounded_effects": "ordered object/global/stack words, full r3, LR/r30/r31 through own frames; 82419178 and FillGuestMemory reuse prior bounded semantics; generic lower ABI and volatile GPR/CR excluded; split-U64 stack representation limited to ordinary guest memory"}
    return manifest, source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    manifest, source = generate()
    encoded = json.dumps(manifest, indent=2) + "\n"
    if args.write:
        write_if_changed(MANIFEST, encoded)
        write_if_changed(SOURCE, source)
    elif MANIFEST.read_text(encoding="utf-8") != encoded or \
            SOURCE.read_text(encoding="utf-8") != source:
        raise ValueError("Linker manifest/source differs from exact cached PPC")
    print("PASS instance-linker 3 complete translated bodies/CFG")


if __name__ == "__main__":
    main()
