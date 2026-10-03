"""One native bounded comparison of five connection/bit-writer PPC bodies."""

import argparse
import json
from pathlib import Path

from generate_instance_connection_initializer_family import reviewed
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-connection-tests")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/"
                           "instance_connection_initializer_families.json")
                          .read_text(encoding="utf-8"))
    if manifest != reviewed(args.source_root):
        raise ValueError("reviewed complete PPC or ABI helper body changed")
    entries = manifest["entries"]
    helpers = manifest["abi_helpers"]
    declarations = "\n".join(f"PPC_FUNC({name});" for name in
                             ["__savegprlr_25", "__restgprlr_25",
                              "__savegprlr_29", "__restgprlr_29",
                              "sub_8267A1F0", "sub_82752768",
                              "sub_823F34B8", "sub_82496948",
                              "sub_8229F678", "sub_82B7BC40"])
    originals = declarations + "\n" + "\n".join(
        entry["translated_body"] for entry in [*helpers, *entries])
    template = (ROOT / "LostOdysseyRecompSemantics/tests/"
                "instance_connection_initializer_oracle.cpp")
    compile_and_run("instance-connection-initializer",
                    originals.encode("utf-8"),
                    template.read_text(encoding="utf-8"),
                    ["LostOdysseyRecompSemantics/src/instance_connection_initializer_family.cpp",
                     "LostOdysseyRecompSemantics/src/string_property_initializer.cpp",
                     "LostOdysseyRecompSemantics/src/instance_linker_composed_family.cpp",
                     "LostOdysseyRecompSemantics/src/instance_tail_initializer_family.cpp",
                     "LostOdysseyRecompSemantics/src/pointer_fields.cpp",
                     "LostOdysseyRecompSemantics/src/registered_metadata_string.cpp",
                     "LostOdysseyRecompSemantics/src/registered_metadata_words.cpp",
                     "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
                     "LostOdysseyRecompSemantics/src/object_registration.cpp",
                     "LostOdysseyRecompSemantics/src/manager_facade.cpp",
                     "LostOdysseyRecompSemantics/src/manager_init.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp",
                     "LostOdysseyRecompSemantics/src/memory_fill.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
