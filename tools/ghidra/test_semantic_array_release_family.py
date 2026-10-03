"""Compare only the new parameterized array-release entrypoints with PPC."""

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run, extract_originals


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-array-release-tests")
    args = parser.parse_args()

    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/array_release_families.json")
                          .read_text(encoding="utf-8"))
    entries = manifest["entries"]
    addresses = [entry["address"] for entry in entries]
    if manifest["schema_version"] != 1 or manifest["kind"] != "array_release_families" or \
            manifest["family_counts"] != {"ReleaseArrayElements": 31} or \
            len(entries) != 31 or addresses != sorted(set(addresses)) or \
            "82298A98" in addresses:
        raise ValueError("array-release manifest changed")
    for entry in entries:
        if not re.fullmatch(r"[0-9A-F]{8}", entry["address"]) or \
                entry["family"] != "ReleaseArrayElements" or \
                entry["evidence"]["branch_target"] != "clear_header" or \
                entry["evidence"]["header_store_order"] != [0, 8, 4]:
            raise ValueError(f"incomplete array-release entry: {entry['address']}")
        params = entry["parameters"]
        sequence = entry["instruction_sequence"]
        if len(sequence) != 23 or sequence[5] != f'li r7,{params["resize_argument"]}' or \
                sequence[6] != f'li r6,{params["element_size"]}' or \
                sequence[12] != f'beq cr6,0x{int(entry["address"], 16) + 56:x}':
            raise ValueError(f"array-release template changed: {entry['address']}")

    originals = extract_originals([
        {"address": entry["address"], "generated_ppc_path": entry["source"],
         "line": entry["line"], "instruction_sequence": entry["instruction_sequence"]}
        for entry in entries
    ], args.ppc_root)
    table = "\n".join(
        f'    {{0x{entry["address"]}u, {entry["parameters"]["element_size"]}, '
        f'{entry["parameters"]["resize_argument"]}, __imp__sub_{entry["address"]}}},'
        for entry in entries
    )
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/array_release_family_oracle.cpp"
               ).read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("array-release oracle table marker changed")
    declarations = b"PPC_FUNC(sub_82298AF8); PPC_FUNC(sub_823F3340);\n"
    compile_and_run("array-release-family", declarations + originals,
                    harness.replace("/* ENTRY_TABLE */", table),
                    ["LostOdysseyRecompSemantics/src/array_release_family.cpp",
                     "LostOdysseyRecompSemantics/src/manager_facade.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
