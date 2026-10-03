"""Compare two shared-metadata initializers and their exact save28 ABI helpers."""

import argparse
import json
from pathlib import Path

from generate_instance_shared_metadata_initializer import reviewed
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-shared-metadata-tests")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/"
                           "instance_shared_metadata_initializer_families.json")
                          .read_text(encoding="utf-8"))
    if manifest != reviewed(args.source_root):
        raise ValueError("reviewed complete PPC or ABI helper body changed")
    declarations = "\n".join(f"PPC_FUNC({name});" for name in
                             ["__savegprlr_28", "__restgprlr_28",
                              "sub_824108C8", "sub_82403148", "sub_82403200"])
    originals = declarations + "\n" + "\n".join(
        entry["translated_body"] for entry in
        [*manifest["abi_helpers"], *manifest["entries"]])
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/"
               "instance_shared_metadata_initializer_oracle.cpp")
    compile_and_run(
        "instance-shared-metadata", originals.encode("utf-8"),
        harness.read_text(encoding="utf-8"),
        ["LostOdysseyRecompSemantics/src/instance_shared_metadata_initializer.cpp",
         "LostOdysseyRecompSemantics/src/registered_callback_family.cpp",
         "LostOdysseyRecompSemantics/src/registered_getter_family.cpp",
         "LostOdysseyRecompSemantics/src/registered_inline_constructor.cpp",
         "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
         "LostOdysseyRecompSemantics/src/object_registration.cpp",
         "LostOdysseyRecompSemantics/src/object_startup.cpp",
         "LostOdysseyRecompSemantics/src/manager_facade.cpp",
         "LostOdysseyRecompSemantics/src/manager_init.cpp",
         "LostOdysseyRecompSemantics/src/allocation_array.cpp",
         "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
