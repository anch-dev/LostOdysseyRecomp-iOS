"""Pin three complete PPC state-array bodies, branches and parameters."""

import argparse
import json
import re
from pathlib import Path

from semantic_batch import ROOT, extract_originals

SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "state_array_processing_families.json"
SOURCE = SEMANTICS / "src/state_array_processing.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
if not (DEFAULT_PPC_ROOT / "ppc_recomp.15.cpp").exists():
    DEFAULT_PPC_ROOT = (Path.home() / "ownCloud/Git/LostOdysseyRecomp"
                        / "LostOdysseyRecompLib/ppc")

LOCATIONS = {
    "823FDAB0": (15, 24239),
    "82407AD8": (15, 48115),
    "8240A7C0": (16, 2586),
}

# Frozen whole-body instruction sequences; updated only after reviewing the
# corresponding exact PPC source, never generated from the source at --check.
EXPECTED = {
    "823FDAB0": ['mflr r12',
     'bl 0x82b7a6cc',
     'stwu r1,-192(r1)',
     'lis r22,-31951',
     'li r21,0',
     'lis r28,-31951',
     'mr r29,r21',
     'mr r27,r21',
     'lwz r11,24292(r22)',
     'mr r31,r21',
     'addi r11,r11,1',
     'stw r29,80(r1)',
     'stw r27,88(r1)',
     'stw r11,24292(r22)',
     'lwz r11,24304(r28)',
     'cmplwi cr6,r11,0',
     'beq cr6,0x823fdb6c',
     'mr r30,r31',
     'addi r31,r31,1',
     'cmpw cr6,r31,r27',
     'stw r31,84(r1)',
     'ble cr6,0x823fdb44',
     'rlwinm r11,r31,1,0,30',
     'li r5,8',
     'add r11,r31,r11',
     'li r4,4',
     'srawi r11,r11,3',
     'addi r3,r1,80',
     'addze r11,r11',
     'add r11,r11,r31',
     'addi r11,r11,32',
     'stw r11,88(r1)',
     'bl 0x8229f678',
     'lwz r27,88(r1)',
     'lwz r31,84(r1)',
     'lwz r29,80(r1)',
     'lwz r11,24304(r28)',
     'rlwinm r10,r30,2,0,29',
     'add r10,r10,r29',
     'cmplwi cr6,r10,0',
     'beq cr6,0x823fdb5c',
     'stw r11,0(r10)',
     'lwz r11,24304(r28)',
     'lwz r11,32(r11)',
     'cmplwi cr6,r11,0',
     'stw r11,24304(r28)',
     'bne cr6,0x823fdaf4',
     'mr r23,r21',
     'cmpwi cr6,r31,0',
     'lis r25,-31951',
     'ble cr6,0x823fdc60',
     'mr r24,r21',
     'lwzx r3,r24,r29',
     'lwz r10,4(r3)',
     'cmpwi cr6,r10,-1',
     'bne cr6,0x823fdba4',
     'lwz r11,0(r3)',
     'lwz r11,124(r11)',
     'mtctr r11',
     'bctrl',
     'lwz r11,24304(r28)',
     'cmplwi cr6,r11,0',
     'beq cr6,0x823fdc48',
     'mr r26,r31',
     'addi r31,r31,1',
     'cmpw cr6,r31,r27',
     'ble cr6,0x823fdc20',
     'rlwinm r10,r31,1,0,30',
     'cmplwi cr6,r29,0',
     'add r10,r31,r10',
     'srawi r10,r10,3',
     'addze r10,r10',
     'add r10,r10,r31',
     'addi r27,r10,32',
     'bne cr6,0x823fdbe4',
     'cmpwi cr6,r27,0',
     'beq cr6,0x823fdc20',
     'lwz r3,-18936(r25)',
     'rlwinm r30,r27,2,0,29',
     'cmplwi cr6,r3,0',
     'bne cr6,0x823fdbfc',
     'bl 0x827c5f38',
     'lwz r3,-18936(r25)',
     'lwz r11,0(r3)',
     'li r6,8',
     'mr r5,r30',
     'mr r4,r29',
     'lwz r11,8(r11)',
     'mtctr r11',
     'bctrl',
     'lwz r11,24304(r28)',
     'mr r29,r3',
     'rlwinm r10,r26,2,0,29',
     'add r10,r10,r29',
     'cmplwi cr6,r10,0',
     'beq cr6,0x823fdc38',
     'stw r11,0(r10)',
     'lwz r11,24304(r28)',
     'lwz r11,32(r11)',
     'cmplwi cr6,r11,0',
     'stw r11,24304(r28)',
     'bne cr6,0x823fdbac',
     'addi r23,r23,1',
     'addi r24,r24,4',
     'cmpw cr6,r23,r31',
     'blt cr6,0x823fdb80',
     'stw r29,80(r1)',
     'stw r27,88(r1)',
     'stw r21,84(r1)',
     'cmpwi cr6,r27,0',
     'beq cr6,0x823fdcac',
     'stw r21,88(r1)',
     'cmplwi cr6,r29,0',
     'beq cr6,0x823fdcac',
     'lwz r3,-18936(r25)',
     'cmplwi cr6,r3,0',
     'bne cr6,0x823fdc8c',
     'bl 0x827c5f38',
     'lwz r3,-18936(r25)',
     'lwz r11,0(r3)',
     'li r6,8',
     'li r5,0',
     'mr r4,r29',
     'lwz r11,8(r11)',
     'mtctr r11',
     'bctrl',
     'stw r3,80(r1)',
     'lwz r11,24292(r22)',
     'li r7,8',
     'li r6,4',
     'addi r11,r11,-1',
     'li r5,0',
     'li r4,0',
     'addi r3,r1,80',
     'stw r11,24292(r22)',
     'bl 0x82298af8',
     'lwz r31,80(r1)',
     'cmplwi cr6,r31,0',
     'beq cr6,0x823fdd04',
     'lwz r3,-18936(r25)',
     'cmplwi cr6,r3,0',
     'bne cr6,0x823fdcf0',
     'bl 0x827c5f38',
     'lwz r3,-18936(r25)',
     'lwz r11,0(r3)',
     'mr r4,r31',
     'lwz r11,12(r11)',
     'mtctr r11',
     'bctrl',
     'addi r1,r1,192',
     'b 0x82b7a71c'],
    "82407AD8": ['mflr r12',
     'stw r12,-8(r1)',
     'std r30,-24(r1)',
     'std r31,-16(r1)',
     'stwu r1,-112(r1)',
     'lis r11,-32256',
     'mr r31,r3',
     'addi r11,r11,15048',
     'li r30,0',
     'stw r11,0(r31)',
     'stw r30,112(r31)',
     'stw r30,116(r31)',
     'stw r30,120(r31)',
     'stw r30,128(r31)',
     'stw r30,132(r31)',
     'stw r30,136(r31)',
     'ld r11,8(r31)',
     'rlwinm r11,r11,0,22,22',
     'cmpldi cr6,r11,0',
     'bne cr6,0x82407b5c',
     'lwz r11,72(r31)',
     'cmpwi cr6,r11,0',
     'bne cr6,0x82407b54',
     'lwz r11,40(r31)',
     'cmpwi cr6,r11,0',
     'bne cr6,0x82407b54',
     'li r11,1',
     'lis r10,-31951',
     'stw r11,72(r31)',
     'stw r11,24284(r10)',
     'bl 0x823fdab0',
     'stw r30,60(r31)',
     'stw r30,84(r31)',
     'mr r3,r31',
     'stw r30,152(r31)',
     'addi r1,r1,112',
     'lwz r12,-8(r1)',
     'mtlr r12',
     'ld r30,-24(r1)',
     'ld r31,-16(r1)',
     'blr'],
    "8240A7C0": ['cmplwi cr6,r3,0', 'beqlr cr6', 'b 0x82407ad8'],
}
PARAMETERS = {
    "823FDAB0": {
        "frame_size": 192, "abi_save": "__savegprlr_21",
        "abi_restore": "__restgprlr_21",
        "counter": "0x83315EE4", "queue_head": "0x83315EF0",
        "manager_global": "0x8330B608",
        "local_header_offsets": [80, 84, 88], "node_next_offset": 32,
        "item_method_offset": 124, "manager_resize_offset": 8,
        "manager_release_offset": 12, "array_element_size": 4,
        "array_allocator_argument": 8,
    },
    "82407AD8": {
        "frame_size": 112, "vtable": "0x82003AC8",
        "flag_mask": "0x200", "global_flag": "0x83315EDC",
        "clear_offsets_before_gate": [112, 116, 120, 128, 132, 136],
        "conditional_clear_offsets": [60, 84], "final_clear_offset": 152,
        "hub_target": "823FDAB0",
    },
    "8240A7C0": {"tail_target": "82407AD8", "guard": "r3.low32 == 0"},
}


def load_body(address, ppc_root):
    unit, line = LOCATIONS[address]
    entry = {"address": address, "line": line,
             "generated_ppc_path":
             f"LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp"}
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
    expected_counts = {"823FDAB0": 151, "82407AD8": 41, "8240A7C0": 3}
    expected_labels = {
        "823FDAB0": ["loc_823FDAF4", "loc_823FDB44", "loc_823FDB5C",
                     "loc_823FDB6C", "loc_823FDB80", "loc_823FDBA4",
                     "loc_823FDBAC", "loc_823FDBE4", "loc_823FDBFC",
                     "loc_823FDC20", "loc_823FDC38", "loc_823FDC48",
                     "loc_823FDC60", "loc_823FDC8C", "loc_823FDCAC",
                     "loc_823FDCF0", "loc_823FDD04"],
        "82407AD8": ["loc_82407B54", "loc_82407B5C"],
        "8240A7C0": [],
    }
    for address, expected in EXPECTED.items():
        body = load_body(address, ppc_root)
        actual = instructions(body)
        if actual != expected or len(actual) != expected_counts[address]:
            raise ValueError(f"unreviewed complete PPC instructions: {address}")
        labels = re.findall(r"^(loc_[0-9A-F]{8}):", body, re.MULTILINE)
        if labels != expected_labels[address]:
            raise ValueError(f"unreviewed CFG labels: {address}")
        unit, line = LOCATIONS[address]
        entries.append({
            "address": address,
            "source": f"LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp",
            "source_line": line,
            "instruction_count": len(expected),
            "parameters": PARAMETERS[address],
            "instructions": expected,
            "calls": [item for item in expected
                      if item.startswith("bl ") or item == "bctrl"],
            "cfg": {"labels": labels,
                    "branches": [item for item in expected if
                                 item.startswith(("beq ", "bne ", "ble ",
                                                  "blt ", "bgt ", "b ",
                                                  "beqlr"))]},
            "translated_body": body,
        })
    if list(EXPECTED) != list(LOCATIONS):
        raise ValueError("state-array entry ordering changed")
    if ((-31951 << 16) + 24292) & 0xffffffff != 0x83315EE4 or \
       ((-31951 << 16) + 24304) & 0xffffffff != 0x83315EF0 or \
       ((-31951 << 16) - 18936) & 0xffffffff != 0x8330B608 or \
       ((-32256 << 16) + 15048) & 0xffffffff != 0x82003AC8:
        raise ValueError("reviewed global/vtable arithmetic changed")
    return {"schema_version": 1, "family": "state_array_processing",
            "entry_count": 3, "entries": entries}


def check_source_contract():
    source = SOURCE.read_text(encoding="utf-8")
    for fragment in ("0x83315EE4u", "0x83315EF0u",
                     "0x83315EDCu", "0x82003AC8u", "GrownCapacity",
                     "CallRemoveArrayRange", "__savegprlr_21"):
        if fragment not in source:
            raise ValueError(f"reviewed C++ source binding missing: {fragment}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    args = parser.parse_args()
    payload = recover(args.ppc_root)
    check_source_contract()
    manifest = json.dumps(payload, indent=2) + "\n"
    if args.check:
        if MANIFEST.read_text(encoding="utf-8") != manifest:
            raise ValueError("state-array manifest stale")
    else:
        MANIFEST.write_text(manifest, encoding="utf-8", newline="\n")
    print("Recovered 3 exact state-array bodies.")


if __name__ == "__main__":
    main()
