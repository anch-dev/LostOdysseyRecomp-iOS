"""Compare three new Linker/archive entries with actual cached PPC chains."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_linker_composed_family import generate
from semantic_batch import ROOT, compile_and_run, extract_originals


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-linker-composed-tests")
    args = parser.parse_args()

    manifest, source = generate()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    if json.loads((semantics / "instance_linker_composed_families.json")
                  .read_text(encoding="utf-8")) != manifest or \
            (semantics / "src/instance_linker_composed_family.cpp")\
                    .read_text(encoding="utf-8") != source:
        raise ValueError("Linker manifest/source differs from exact cached PPC")
    if [entry["address"] for entry in manifest["entries"]] != \
            ["823F34B8", "824190F8", "82419230"]:
        raise ValueError("Linker target set changed")
    lower_manifest = json.loads((semantics / "instance_tail_initializer_families.json")
                                .read_text(encoding="utf-8"))
    lower = next(entry for entry in lower_manifest["entries"] if
                 entry["address"] == "82419178")
    fill = extract_originals([{"address": "82B7BC40",
                               "generated_ppc_path":
                               "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp",
                               "line": 10912}],
                             ROOT / "LostOdysseyRecompLib/ppc")
    declarations = "\n".join(f"PPC_FUNC(sub_{address});" for address in
                             ["82B7BC40", "82419178", "823F34B8"])
    bodies = (["\n".join(lower["body"])] +
              ["\n".join(entry["body"]) for entry in manifest["entries"]])
    originals = declarations.encode("utf-8") + b"\n" + fill + b"\n" + \
        "\n".join(bodies).encode("utf-8")
    harness = (semantics / "tests/instance_linker_composed_oracle.cpp")\
        .read_text(encoding="utf-8")
    compile_and_run("instance-linker-composed", originals, harness,
                    ["LostOdysseyRecompSemantics/src/instance_linker_composed_family.cpp",
                     "LostOdysseyRecompSemantics/src/instance_tail_initializer_family.cpp",
                     "LostOdysseyRecompSemantics/src/memory_fill.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp",
                     "LostOdysseyRecompSemantics/src/pointer_fields.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
