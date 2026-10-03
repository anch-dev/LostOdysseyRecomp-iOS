"""Bounded native PPC comparison for bit-array/World composition."""

import argparse
import json
from pathlib import Path

from generate_instance_allocation_composed_family import reviewed, extract
from generate_string_property_initializer import recover as string_reviewed
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-instance-allocation-composed-tests")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/"
        "instance_allocation_composed_families.json").read_text(encoding="utf-8"))
    if manifest != reviewed(args.source_root):
        raise ValueError("reviewed PPC bit-array/World body changed")
    string_manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/"
        "string_property_initializer_families.json").read_text(encoding="utf-8"))
    if string_manifest != string_reviewed(args.source_root / "LostOdysseyRecompLib/ppc"):
        raise ValueError("reused property baseline changed")
    symbols = ["__savegprlr_28", "__restgprlr_28", "sub_823058F0", "sub_825BA620",
        "sub_82496948", "sub_822A06C0", "sub_8229F678", "sub_82B7A0B0",
        "sub_8229C8B0", "sub_82298938", "sub_82B7BC40", "sub_827C5F38",
        "sub_823ACBD0", "sub_827C5970", "sub_827C4ED0"]
    bodies = [*manifest["abi_helpers"], *manifest["entries"],
              *string_manifest["entries"], extract(args.source_root,
                  "ppc_recomp.75.cpp", 11141, "sub_827C5F38")]
    dispatch = """static void Dispatch(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) Dispatch(ctx, base, address)
"""
    originals = dispatch + "\n".join(f"PPC_FUNC({name});" for name in symbols) + "\n" + \
        "\n".join(entry["translated_body"] for entry in bodies)
    compile_and_run("instance-allocation-composed", originals.encode(),
        (ROOT / "LostOdysseyRecompSemantics/tests/instance_allocation_composed_oracle.cpp")
            .read_text(encoding="utf-8"),
        ["LostOdysseyRecompSemantics/src/" + name + ".cpp" for name in (
            "instance_allocation_composed_family", "string_property_initializer",
            "registered_metadata_string", "registered_metadata_words",
            "registered_constructor_family", "object_registration",
            "manager_facade", "manager_init",
            "allocation_array", "memory_move", "memory_fill")], args.output)


if __name__ == "__main__":
    main()
