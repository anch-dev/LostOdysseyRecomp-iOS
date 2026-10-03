"""Recover five exact metadata-range bodies from recorded PPC locations.

Only the recorded source files and four cached parent entries are read. The
instruction templates reject an unreviewed body; no source tree scan or hash.
"""

import argparse
import json
import re
from pathlib import Path

from semantic_batch import ROOT, extract_originals

SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
if not (DEFAULT_PPC_ROOT / "ppc_recomp.25.cpp").exists():
    DEFAULT_PPC_ROOT = (Path.home() / "ownCloud/Git/LostOdysseyRecomp"
                        / "LostOdysseyRecompLib/ppc")
MANIFEST = SEMANTICS / "registered_metadata_range_families.json"
SOURCE = SEMANTICS / "src/registered_metadata_range.cpp"
BEGIN = "    // BEGIN GENERATED METADATA RANGE PARAMETERS"
END = "    // END GENERATED METADATA RANGE PARAMETERS"

# The four distinct calling orders and literal payloads are reviewed against
# the full cached PPC instruction streams below.
PARENTS = {
    "82412E88": (0x82412ECC, 184, 40, 0x0001003C, (0x00010020,)),
    "825F45F0": (0x825F4618, 60, 60, 0, (0x00010008,)),
    "826B06E8": (0x826B072C, 128, 56, 0x0001003C,
                 (0x00010000, 0x0001001C)),
    "826F8810": (0x826F8838, 60, 100, 0,
                 (0x00010040, 0x00010044)),
}
HELPER = {"address": "8249B130",
          "generated_ppc_path": "LostOdysseyRecompLib/ppc/ppc_recomp.25.cpp",
          "line": 19747}
PROLOGUE = ["mflr r12", "stw r12,-8(r1)", "std r30,-24(r1)",
            "std r31,-16(r1)", "stwu r1,-112(r1)"]
EPILOGUE = ["addi r1,r1,112", "lwz r12,-8(r1)", "mtlr r12",
            "ld r30,-24(r1)", "ld r31,-16(r1)", "blr"]
BACKFILL = [
    "lwz r11,4(r31)", "rlwinm r8,r30,2,0,29", "lwz r10,0(r31)",
    "rlwinm r11,r11,2,0,29", "add r11,r11,r10", "lwz r10,-4(r11)",
    "rlwinm r9,r10,0,0,7", "addis r9,r9,256",
    "rlwimi r9,r10,0,8,31", "stw r9,-4(r11)", "lwz r11,4(r31)",
    "lwz r10,0(r31)", "rlwinm r9,r11,2,0,29", "subf r7,r30,r11",
    "add r11,r9,r10", "lbz r11,-4(r11)", "addi r11,r11,-1",
    "rlwimi r7,r11,24,0,7", "stwx r7,r8,r10",
]


def instruction_comments(body):
    return [line.strip() for line in re.findall(r"^\s*//\s*(.*?)\s*$", body,
                                               re.MULTILINE)]


def load_body(entry, ppc_root=DEFAULT_PPC_ROOT):
    body = extract_originals([entry], ppc_root)
    text = body.decode("utf-8").replace("\r\n", "\n")
    if not text.startswith(f"PPC_FUNC_IMPL(__imp__sub_{entry['address']}) {{") or \
            not text.rstrip().endswith("}") or text.count("PPC_FUNC_IMPL(") != 1:
        raise ValueError(f"whole PPC body changed: {entry['address']}")
    return text


def helper_expected():
    return PROLOGUE + [
        "clrlwi r11,r4,16", "addi r31,r3,364", "oris r11,r11,4",
        "addi r4,r1,80", "mr r3,r31", "mr r30,r5", "stw r11,80(r1)",
        "bl 0x825f41e8", "addi r4,r1,80", "stw r30,80(r1)",
        "mr r3,r31", "bl 0x825f41e8", "lis r11,-8531",
        "addi r4,r1,80", "ori r11,r11,47806", "mr r3,r31",
        "stw r11,80(r1)", "bl 0x825f41e8",
    ] + EPILOGUE


def parent_expected(address):
    _, offset, size, prefix, suffix = PARENTS[address]
    if prefix:
        result = ["lis r11,1", "lwz r30,52(r3)", "addi r4,r1,80",
                  f"ori r11,r11,{prefix & 0xffff}", "addi r31,r30,364",
                  "mr r3,r31", "stw r11,80(r1)", "bl 0x825f41e8",
                  f"li r5,{size}", f"li r4,{offset}", "mr r3,r30",
                  "bl 0x8249b130", "lis r11,1", "mr r30,r3"]
    else:
        result = ["lwz r31,52(r3)", f"li r5,{size}",
                  f"li r4,{offset}", "mr r3,r31", "bl 0x8249b130",
                  "lis r11,1", "addi r31,r31,364",
                  f"ori r11,r11,{suffix[0] & 0xffff}", "mr r30,r3"]
    for index, word in enumerate(suffix):
        if index:
            result += ["lis r11,1", "addi r4,r1,80",
                       f"ori r11,r11,{word & 0xffff}", "mr r3,r31",
                       "stw r11,80(r1)", "bl 0x825f41e8"]
            continue
        if prefix and (word & 0xffff):
            result.append(f"ori r11,r11,{word & 0xffff}")
        result += ["addi r4,r1,80", "mr r3,r31", "stw r11,80(r1)",
                   "bl 0x825f41e8"]
    return PROLOGUE + result + BACKFILL + EPILOGUE


def recover(ppc_root=DEFAULT_PPC_ROOT):
    cached = json.loads((ROOT / "out/function-inventory/registered-extra-methods.json")
                        .read_text(encoding="utf-8"))
    parents = {entry["address"]: entry for entry in cached
               if entry["address"] in PARENTS}
    if set(parents) != set(PARENTS):
        raise ValueError("reviewed metadata-range parent set changed")
    entries = []
    for address in sorted([HELPER["address"], *PARENTS]):
        if address == HELPER["address"]:
            entry, expected = HELPER, helper_expected()
        else:
            entry, expected = parents[address], parent_expected(address)
        body = load_body(entry, ppc_root)
        instructions = instruction_comments(body)
        if instructions != expected:
            raise ValueError(f"unreviewed full instruction structure: {address}")
        if address != HELPER["address"] and \
                body.rstrip("\n") != entry["body"].replace("\r\n", "\n"):
            raise ValueError(f"cached full PPC body changed: {address}")
        calls = [item for item in instructions if item.startswith("bl ")]
        metadata = {"address": address,
                    "source": entry["generated_ppc_path"],
                    "source_line": entry["line"],
                    "instruction_count": len(instructions),
                    "cfg": [], "calls": calls,
                    "instructions": instructions}
        if address == HELPER["address"]:
            metadata["kind"] = "range_helper"
        else:
            helper_return, offset, size, prefix, suffix = PARENTS[address]
            metadata.update(kind="range_parent", helper_return=helper_return,
                            offset=offset, size=size, prefix=prefix,
                            suffix=[f"0x{word:08X}" for word in suffix])
        entries.append(metadata)
    return {"schema_version": 1, "family": "registered_metadata_range",
            "source": "recorded PPC locations and registered-extra-methods.json",
            "entry_count": len(entries), "entries": entries}


def generated_source(payload):
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("C++ parameter markers changed")
    prefix, tail = source.split(BEGIN, 1)
    _, suffix = tail.split(END, 1)
    rows = []
    for entry in payload["entries"]:
        if entry["kind"] == "range_parent":
            words = [int(word, 16) for word in entry["suffix"]]
            rows.append("    {0x%s, 0x%X, %u, %u, 0x%08X, %u, {%s}}," %
                        (entry["address"], entry["helper_return"],
                         entry["offset"], entry["size"], entry["prefix"],
                         len(words), ", ".join("0x%08X" % w for w in words)))
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
            raise ValueError("metadata-range manifest or parameter table stale")
    else:
        MANIFEST.write_text(manifest, encoding="utf-8", newline="\n")
        SOURCE.write_text(source, encoding="utf-8", newline="\n")
    print(f"Recovered {payload['entry_count']} exact metadata-range bodies.")


if __name__ == "__main__":
    main()
