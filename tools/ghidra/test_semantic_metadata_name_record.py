"""Compare the recovered named-record constructor with its exact PPC body."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_metadata_name_record import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-metadata-name-record-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "metadata_name_record_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("metadata name record manifest changed")
    declarations = """
void SaveGprs(PPCContext&, std::uint8_t*);
void RestoreGprs(PPCContext&, std::uint8_t*);
void OriginalDirectCall(PPCContext&, std::uint8_t*, std::uint32_t);
void NameRecordIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
#define __savegprlr_27(ctx, base) SaveGprs(ctx, base)
#define __restgprlr_27(ctx, base) RestoreGprs(ctx, base)
#define sub_82296830(ctx, base) OriginalDirectCall(ctx, base, 0x82296830u)
#define sub_827C5F38(ctx, base) OriginalDirectCall(ctx, base, 0x827c5f38u)
#define sub_8230BAC0(ctx, base) OriginalDirectCall(ctx, base, 0x8230bac0u)
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) NameRecordIndirect(ctx, base, address)
"""
    originals = declarations + "\n" + manifest["entries"][0]["translated_body"]
    harness = (semantics / "tests/metadata_name_record_oracle.cpp")\
        .read_text(encoding="utf-8")
    sources = [
        "src/metadata_name_record.cpp", "src/registered_metadata_composed.cpp",
        "src/registered_metadata_string.cpp", "src/manager_init.cpp",
        "src/registered_metadata_words.cpp", "src/registered_constructor_family.cpp",
        "src/registered_callback_family.cpp", "src/registered_getter_family.cpp",
        "src/registered_inline_constructor.cpp", "src/object_registration.cpp",
        "src/object_startup.cpp", "src/manager_facade.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
    ]
    compile_and_run("metadata-name-record", originals.encode("utf-8"),
                    harness, [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
