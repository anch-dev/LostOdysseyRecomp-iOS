"""Compare 82296D30 with its exact PPC body and validated lower models."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_metadata_name_lookup import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-metadata-name-lookup-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "metadata_name_lookup_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("metadata name lookup manifest changed")
    declarations = """
void SaveGprs(PPCContext&, std::uint8_t*);
void RestoreGprs(PPCContext&, std::uint8_t*);
void OriginalDirectCall(PPCContext&, std::uint8_t*, std::uint32_t);
#define __savegprlr_26(ctx, base) SaveGprs(ctx, base)
#define __restgprlr_26(ctx, base) RestoreGprs(ctx, base)
#define sub_823F4700(ctx, base) OriginalDirectCall(ctx, base, 0x823f4700u)
#define sub_82296E80(ctx, base) OriginalDirectCall(ctx, base, 0x82296e80u)
#define sub_82296F68(ctx, base) OriginalDirectCall(ctx, base, 0x82296f68u)
#define sub_822971E0(ctx, base) OriginalDirectCall(ctx, base, 0x822971e0u)
#define sub_822C42D8(ctx, base) OriginalDirectCall(ctx, base, 0x822c42d8u)
#define sub_823F7B08(ctx, base) OriginalDirectCall(ctx, base, 0x823f7b08u)
#define sub_8230BAC0(ctx, base) OriginalDirectCall(ctx, base, 0x8230bac0u)
"""
    originals = declarations + "\n" + manifest["entries"][0]["translated_body"]
    harness = (semantics / "tests/metadata_name_lookup_oracle.cpp")\
        .read_text(encoding="utf-8")
    sources = [
        "src/metadata_name_lookup.cpp", "src/metadata_name_registry.cpp",
        "src/metadata_name_index.cpp", "src/metadata_name_record.cpp",
        "src/manager_metadata_parsing.cpp", "src/manager_metadata_compare.cpp",
        "src/manager_object_registration.cpp", "src/registered_metadata_string.cpp",
        "src/registered_metadata_composed.cpp", "src/registered_metadata_words.cpp",
        "src/registered_constructor_family.cpp", "src/registered_callback_family.cpp",
        "src/registered_getter_family.cpp", "src/registered_inline_constructor.cpp",
        "src/object_registration.cpp", "src/object_startup.cpp",
        "src/manager_facade.cpp", "src/manager_init.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
        "src/allocation_failure.cpp", "src/crt_thread_data.cpp",
        "src/invalid_parameter.cpp",
    ]
    compile_and_run("metadata-name-lookup", originals.encode("utf-8"),
                    harness, [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
