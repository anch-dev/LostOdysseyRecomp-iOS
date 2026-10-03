"""Pin both exact string-initializer bodies and their ordered parameters."""

import argparse
import json
import re
from pathlib import Path

from semantic_batch import ROOT, extract_originals

SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "instance_string_initializer_families.json"
SOURCE = SEMANTICS / "src/instance_string_initializer_family.cpp"
BEGIN = "    // BEGIN GENERATED INSTANCE STRING PARAMETERS"
END = "    // END GENERATED INSTANCE STRING PARAMETERS"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
if not (DEFAULT_PPC_ROOT / "ppc_recomp.15.cpp").exists():
    DEFAULT_PPC_ROOT = (Path.home() / "ownCloud/Git/LostOdysseyRecomp"
                        / "LostOdysseyRecompLib/ppc")

DIRECT = {"address": "82407300",
          "generated_ppc_path": "LostOdysseyRecompLib/ppc/ppc_recomp.15.cpp",
          "line": 46884}
TAIL_ADDRESS = "8240AE28"
DIRECT_INSTRUCTIONS = [
    "mflr r12", "stw r12,-8(r1)", "std r31,-16(r1)",
    "stwu r1,-96(r1)", "lis r11,-32231", "lis r10,-32231",
    "lis r9,-32231", "mr r31,r3", "addi r11,r11,-26384",
    "addi r10,r10,3728", "addi r9,r9,3992", "lis r8,-32229",
    "addi r3,r31,72", "addi r4,r8,-31792", "stw r11,60(r31)",
    "stw r10,0(r31)", "stw r9,60(r31)", "bl 0x8229c8b0",
    "mr r3,r31", "addi r1,r1,96", "lwz r12,-8(r1)",
    "mtlr r12", "ld r31,-16(r1)", "blr",
]
TAIL_INSTRUCTIONS = [
    "cmplwi cr6,r3,0", "beqlr cr6", "lis r11,-32229",
    "addi r4,r11,-31792", "b 0x82407300",
]


def load_body(entry, ppc_root=DEFAULT_PPC_ROOT):
    body = extract_originals([entry], ppc_root).decode("utf-8")
    body = body.replace("\r\n", "\n")
    if not body.startswith(f"PPC_FUNC_IMPL(__imp__sub_{entry['address']}) {{") or \
            not body.rstrip().endswith("}") or body.count("PPC_FUNC_IMPL(") != 1:
        raise ValueError(f"whole source body changed: {entry['address']}")
    return body


def instructions(body):
    return [item.strip() for item in
            re.findall(r"^\s*//\s*(.*?)\s*$", body, re.MULTILINE)]


def recover(ppc_root=DEFAULT_PPC_ROOT):
    candidates = json.loads((ROOT / "out/function-inventory"
                             / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))["entries"]
    tails = [entry for entry in candidates
             if entry["address"] == TAIL_ADDRESS]
    if len(tails) != 1:
        raise ValueError("reviewed string tail candidate changed")
    entries = []
    for record, expected, kind, parameters in [
        (DIRECT, DIRECT_INSTRUCTIONS, "direct",
         {"frame_size": 96, "first_vtable": "0x821898F0",
          "base_vtable": "0x82190E90", "final_vtable": "0x82190F98",
          "string_offset": 72,
          "source_register": "0xFFFFFFFF821A83D0"}),
        (tails[0], TAIL_INSTRUCTIONS, "null_guarded_tail",
         {"tail_target": DIRECT["address"],
          "source_register": "0xFFFFFFFF821A83D0"}),
    ]:
        body = load_body(record, ppc_root)
        if instructions(body) != expected:
            raise ValueError(f"unreviewed complete PPC instructions: {record['address']}")
        if kind == "null_guarded_tail" and \
                instructions(body) != [item.strip() for item in record["instructions"]]:
            raise ValueError("cached null-guarded tail instructions changed")
        if kind == "direct" and \
                (0x82190000 - 26384, 0x82190000 + 3728,
                 0x82190000 + 3992, (0x821B0000 - 31792) & 0xffffffff) != \
                tuple(int(parameters[key], 16) for key in
                      ("first_vtable", "base_vtable", "final_vtable")) + \
                (0x821A83D0,):
            raise ValueError("reviewed vtables/source arithmetic changed")
        entries.append({
            "address": record["address"],
            "source": record["generated_ppc_path"],
            "source_line": record["line"],
            "kind": kind,
            "parameters": parameters,
            "instruction_count": len(expected),
            "instructions": expected,
            "calls": [item for item in expected if item.startswith("bl ")],
            "cfg": {"labels": [], "branches": [item for item in expected
                                              if item.startswith(("beq", "b "))]},
            "translated_body": body,
        })
    return {"schema_version": 1, "family": "instance_string_initializer",
            "entry_count": 2, "entries": entries}


def generated_source(payload):
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("C++ parameter markers changed")
    prefix, tail = source.split(BEGIN, 1)
    _, suffix = tail.split(END, 1)
    rows = []
    for entry in payload["entries"]:
        p = entry["parameters"]
        if entry["kind"] == "direct":
            row = f"    {{0x{entry['address']}u, false, {p['first_vtable']}u, " \
                  f"{p['base_vtable']}u, {p['final_vtable']}u, " \
                  f"{p['source_register']}ull}},"
        else:
            row = f"    {{0x{entry['address']}u, true, 0, 0, 0, " \
                  f"{p['source_register']}ull}},"
        rows.append(row)
    return prefix + BEGIN + "\n" + "\n".join(rows) + "\n" + END + suffix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    args = parser.parse_args()
    payload = recover(args.ppc_root)
    manifest = json.dumps(payload, indent=2) + "\n"
    source = generated_source(payload)
    if args.check:
        if MANIFEST.read_text(encoding="utf-8") != manifest or \
                SOURCE.read_text(encoding="utf-8") != source:
            raise ValueError("string-initializer manifest or parameters stale")
    else:
        MANIFEST.write_text(manifest, encoding="utf-8", newline="\n")
        SOURCE.write_text(source, encoding="utf-8", newline="\n")
    print("Recovered 2 exact string-initializer bodies.")


if __name__ == "__main__":
    main()
