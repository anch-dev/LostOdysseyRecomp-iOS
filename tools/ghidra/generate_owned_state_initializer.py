"""Pin both complete PPC owned-state bodies and their reviewed parameters."""

import argparse
import json
import re
from pathlib import Path

from semantic_batch import ROOT, extract_originals

SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "owned_state_initializer_families.json"
SOURCE = SEMANTICS / "src/owned_state_initializer.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
if not (DEFAULT_PPC_ROOT / "ppc_recomp.15.cpp").exists():
    DEFAULT_PPC_ROOT = (Path.home() / "ownCloud/Git/LostOdysseyRecomp"
                        / "LostOdysseyRecompLib/ppc")

SOURCES = {
    "82406A38": ("LostOdysseyRecompLib/ppc/ppc_recomp.15.cpp", 45485),
    "823FA008": ("LostOdysseyRecompLib/ppc/ppc_recomp.15.cpp", 15625),
}
LEAF = [
    "lis r11,-32231", "cmplwi cr6,r4,0", "addi r11,r11,-26280",
    "stw r11,0(r3)", "li r11,0", "beq cr6,0x82406a58",
    "lwz r9,52(r4)", "b 0x82406a5c", "mr r9,r11",
    "lis r10,-32231", "stw r9,4(r3)", "stw r4,8(r3)",
    "li r9,-1", "addi r10,r10,-10068", "stw r11,12(r3)",
    "stw r11,16(r3)", "stw r11,20(r3)", "stw r11,24(r3)",
    "stw r10,0(r3)", "lwz r10,52(r4)", "std r9,32(r3)",
    "stw r10,28(r3)", "stw r11,44(r3)", "stw r11,48(r3)",
    "stw r11,52(r3)", "blr",
]
HUB = [
    "mflr r12", "stw r12,-8(r1)", "std r31,-16(r1)",
    "stwu r1,-96(r1)", "mr r31,r3", "lwz r3,24(r31)",
    "cmplwi cr6,r3,0", "beq cr6,0x823fa03c",
    "lwz r11,0(r3)", "li r4,1", "lwz r11,0(r11)",
    "mtctr r11", "bctrl", "li r3,56", "bl 0x82486c88",
    "cmplwi cr6,r3,0", "beq cr6,0x823fa058",
    "mr r4,r31", "bl 0x82406a38", "b 0x823fa05c",
    "li r3,0", "li r12,1", "ld r11,8(r31)",
    "stw r3,24(r31)", "rldicr r12,r12,57,63",
    "or r11,r11,r12", "std r11,8(r31)",
    "addi r1,r1,96", "lwz r12,-8(r1)", "mtlr r12",
    "ld r31,-16(r1)", "blr",
]


def load_body(address, ppc_root):
    path, line = SOURCES[address]
    entry = {"address": address, "generated_ppc_path": path, "line": line}
    body = extract_originals([entry], ppc_root).decode("utf-8").replace("\r\n", "\n")
    if not body.startswith(f"PPC_FUNC_IMPL(__imp__sub_{address}) {{") or \
            not body.rstrip().endswith("}") or body.count("PPC_FUNC_IMPL(") != 1:
        raise ValueError(f"whole source body changed: {address}")
    return body


def instructions(body):
    return [item.strip() for item in
            re.findall(r"^\s*//\s*(.*?)\s*$", body, re.MULTILINE)]


def recover(ppc_root=DEFAULT_PPC_ROOT):
    entries = []
    for address, expected, kind, parameters, labels in [
        ("82406A38", LEAF, "initialize_state",
         {"initial_vtable": "0x82189958", "final_vtable": "0x8218D8AC",
          "owner_word_offset": 52, "owner_pointer_offset": 8,
          "sentinel_offset": 32, "sentinel": "0xFFFFFFFFFFFFFFFF"},
         ["loc_82406A58", "loc_82406A5C"]),
        ("823FA008", HUB, "replace_state",
         {"frame_size": 96, "old_state_offset": 24,
          "old_vtable_slot": 0, "destroy_argument": 1,
          "allocation_bytes": 56, "owner_flag_bit": 57,
          "allocation_target": "82486C88", "initializer_target": "82406A38"},
         ["loc_823FA03C", "loc_823FA058", "loc_823FA05C"]),
    ]:
        body = load_body(address, ppc_root)
        if instructions(body) != expected:
            raise ValueError(f"unreviewed complete PPC instructions: {address}")
        actual_labels = re.findall(r"^(loc_[0-9A-F]{8}):", body, re.MULTILINE)
        if actual_labels != labels:
            raise ValueError(f"unreviewed CFG labels: {address}")
        path, line = SOURCES[address]
        entries.append({
            "address": address,
            "source": path,
            "source_line": line,
            "kind": kind,
            "instruction_count": len(expected),
            "parameters": parameters,
            "calls": [item for item in expected if item.startswith("bl ") or
                      item == "bctrl"],
            "cfg": {"labels": labels, "branches": [item for item in expected
                                                if item.startswith(("beq ", "b "))]},
            "instructions": expected,
            "translated_body": body,
        })
    if ((-32231 << 16) - 26280) & 0xffffffff != 0x82189958 or \
            ((-32231 << 16) - 10068) & 0xffffffff != 0x8218D8AC:
        raise ValueError("reviewed state vtable arithmetic changed")
    return {"schema_version": 1, "family": "owned_state_initializer",
            "entry_count": 2, "entries": entries}


def check_source_contract():
    source = SOURCE.read_text(encoding="utf-8")
    for text in ("0x82189958u", "0x8218D8ACu", "source + 52u",
                 "UINT64_MAX", "memory.ReadU32(source + 52u)",
                 "old_state", "vtable", "& ~3u", "old_state, 1",
                 "AllocateManagerBuffer(memory, manager_services,",
                 "56, sp - 96u", "std::uint64_t{1} << 57u"):
        if text not in source:
            raise ValueError(f"reviewed C++ parameter missing: {text}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    args = parser.parse_args()
    payload = recover(args.ppc_root)
    manifest = json.dumps(payload, indent=2) + "\n"
    check_source_contract()
    if args.check:
        if MANIFEST.read_text(encoding="utf-8") != manifest:
            raise ValueError("owned-state manifest stale")
    else:
        MANIFEST.write_text(manifest, encoding="utf-8", newline="\n")
    print("Recovered 2 exact owned-state bodies.")


if __name__ == "__main__":
    main()
