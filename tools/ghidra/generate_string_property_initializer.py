"""Pin two exact PPC string-property foundations and their constants."""

import argparse
import json
import re
from pathlib import Path

from semantic_batch import ROOT, extract_originals

SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "string_property_initializer_families.json"
SOURCE = SEMANTICS / "src/string_property_initializer.cpp"
BEGIN = "    // BEGIN GENERATED STRING PROPERTY PARAMETERS"
END = "    // END GENERATED STRING PROPERTY PARAMETERS"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
if not (DEFAULT_PPC_ROOT / "ppc_recomp.25.cpp").exists():
    DEFAULT_PPC_ROOT = (Path.home() / "ownCloud/Git/LostOdysseyRecomp"
                        / "LostOdysseyRecompLib/ppc")

SOURCES = {
    "822A06C0": ("LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp", 39967),
    "82496948": ("LostOdysseyRecompLib/ppc/ppc_recomp.25.cpp", 9053),
}
COPY = [
    "mflr r12", "stw r12,-8(r1)", "std r30,-24(r1)",
    "std r31,-16(r1)", "stwu r1,-112(r1)", "mr r30,r4",
    "mr r31,r3", "li r10,0", "li r5,8", "li r4,2",
    "lwz r11,4(r30)", "stw r10,0(r31)", "stw r11,4(r31)",
    "stw r11,8(r31)", "bl 0x8229f678", "lwz r11,4(r31)",
    "cmpwi cr6,r11,0", "beq cr6,0x822a0718",
    "rlwinm r5,r11,1,0,30", "lwz r4,0(r30)",
    "lwz r3,0(r31)", "bl 0x82b7a0b0", "mr r3,r31",
    "addi r1,r1,112", "lwz r12,-8(r1)", "mtlr r12",
    "ld r30,-24(r1)", "ld r31,-16(r1)", "blr",
]
PROPERTY = [
    "mflr r12", "bl 0x82b7a6e8", "stwu r1,-144(r1)",
    "li r28,0", "lis r11,-31945", "mr r29,r28", "mr r30,r4",
    "addi r4,r11,-22380", "mr r31,r3", "stw r29,80(r1)",
    "bl 0x822a06c0", "lis r11,-31945", "addi r3,r31,12",
    "addi r4,r11,-22320", "bl 0x822a06c0", "lis r11,-31950",
    "cmplwi cr6,r30,0", "lwz r11,-32596(r11)", "stw r11,24(r31)",
    "beq cr6,0x824969b0", "mr r4,r30", "addi r3,r1,88",
    "li r29,1", "bl 0x8229c8b0", "mr r4,r3", "b 0x824969b8",
    "lis r11,-31945", "addi r4,r11,-22356", "addi r3,r31,28",
    "bl 0x822a06c0", "clrlwi r11,r29,31", "cmpwi cr6,r11,0",
    "beq cr6,0x824969d4", "addi r3,r1,88", "bl 0x82298938",
    "lis r11,-31945", "stw r28,40(r31)", "addi r3,r31,52",
    "stw r28,44(r31)", "addi r4,r11,-22308",
    "stw r28,48(r31)", "bl 0x822a06c0", "li r11,1",
    "mr r3,r31", "stw r11,64(r31)", "addi r1,r1,144",
    "b 0x82b7a738",
]
PARAMETERS = {
    "frame_size": 144,
    "first_header": "0x8336A894",
    "second_header": "0x8336A8D0",
    "default_header": "0x8336A8AC",
    "fourth_header": "0x8336A8DC",
    "global_word": "0x833180AC",
    "temporary_offset": 88,
}


def load_body(address, ppc_root=DEFAULT_PPC_ROOT):
    path, line = SOURCES[address]
    entry = {"address": address, "generated_ppc_path": path, "line": line}
    body = extract_originals([entry], ppc_root).decode("utf-8")
    body = body.replace("\r\n", "\n")
    if not body.startswith(f"PPC_FUNC_IMPL(__imp__sub_{address}) {{") or \
            not body.rstrip().endswith("}") or body.count("PPC_FUNC_IMPL(") != 1:
        raise ValueError(f"whole source body changed: {address}")
    return body


def instructions(body):
    return [item.strip() for item in
            re.findall(r"^\s*//\s*(.*?)\s*$", body, re.MULTILINE)]


def recover(ppc_root=DEFAULT_PPC_ROOT):
    entries = []
    for address, expected in (("822A06C0", COPY), ("82496948", PROPERTY)):
        body = load_body(address, ppc_root)
        if instructions(body) != expected:
            raise ValueError(f"unreviewed exact PPC instruction structure: {address}")
        path, line = SOURCES[address]
        entries.append({
            "address": address,
            "source": path,
            "source_line": line,
            "kind": "copy_string" if address == "822A06C0" else "string_property",
            "instruction_count": len(expected),
            "parameters": {"frame_size": 112} if address == "822A06C0" else PARAMETERS,
            "calls": [item for item in expected if item.startswith("bl ")],
            "cfg": {"labels": ["loc_822A0718"] if address == "822A06C0" else
                    ["loc_824969B0", "loc_824969B8", "loc_824969D4"],
                    "branches": [item for item in expected if
                                 item.startswith(("beq ", "b "))]},
            "instructions": expected,
            "translated_body": body,
        })
    expected_headers = tuple(((base << 16) + offset) & 0xffffffff
                             for base, offset in [
                                 (-31945, -22380), (-31945, -22320),
                                 (-31945, -22356), (-31945, -22308),
                                 (-31950, -32596)])
    if expected_headers != tuple(int(PARAMETERS[name], 16) for name in
                                  ("first_header", "second_header",
                                   "default_header", "fourth_header",
                                   "global_word")):
        raise ValueError("reviewed constant-header arithmetic changed")
    return {"schema_version": 1, "family": "string_property_initializer",
            "entry_count": 2, "entries": entries}


def generated_source(payload):
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("C++ parameter markers changed")
    prefix, tail = source.split(BEGIN, 1)
    _, suffix = tail.split(END, 1)
    p = payload["entries"][1]["parameters"]
    row = "    %su, %su, %su, %su, %su," % tuple(
        p[name] for name in ("first_header", "second_header",
                         "default_header", "fourth_header", "global_word"))
    return prefix + BEGIN + "\n" + row + "\n" + END + suffix


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
            raise ValueError("string-property manifest or constants stale")
    else:
        MANIFEST.write_text(manifest, encoding="utf-8", newline="\n")
        SOURCE.write_text(source, encoding="utf-8", newline="\n")
    print("Recovered 2 exact string-property bodies.")


if __name__ == "__main__":
    main()
