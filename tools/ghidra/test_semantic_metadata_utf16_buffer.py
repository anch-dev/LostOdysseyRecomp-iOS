"""Compare two UTF-16 buffer entries with exact original PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_metadata_utf16_buffer import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-metadata-utf16-buffer-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "metadata_utf16_buffer_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("metadata UTF16 buffer manifest changed")
    declarations = """
void Save29(PPCContext&, std::uint8_t*);
void Restore29(PPCContext&, std::uint8_t*);
void OriginalDirectCall(PPCContext&, std::uint8_t*, std::uint32_t);
#define __savegprlr_29(ctx, base) Save29(ctx, base)
#define __restgprlr_29(ctx, base) Restore29(ctx, base)
#define sub_82296830(ctx, base) OriginalDirectCall(ctx, base, 0x82296830u)
#define sub_822C42D8(ctx, base) OriginalDirectCall(ctx, base, 0x822c42d8u)
#define sub_8230BAC0(ctx, base) OriginalDirectCall(ctx, base, 0x8230bac0u)
#define sub_822A06C0(ctx, base) OriginalDirectCall(ctx, base, 0x822a06c0u)
#define sub_8232D378(ctx, base) OriginalDirectCall(ctx, base, 0x8232d378u)
#define sub_8229F678(ctx, base) OriginalDirectCall(ctx, base, 0x8229f678u)
#define sub_82298A98(ctx, base) OriginalDirectCall(ctx, base, 0x82298a98u)
"""
    originals = declarations + "\n" + "\n".join(
        entry["translated_body"] for entry in manifest["entries"])
    harness = (semantics / "tests/metadata_utf16_buffer_oracle.cpp")\
        .read_text(encoding="utf-8")
    sources = [
        "src/metadata_utf16_buffer.cpp", "src/string_property_initializer.cpp",
        "src/registered_metadata_string.cpp", "src/registered_metadata_words.cpp",
        "src/registered_metadata_composed.cpp",
        "src/registered_constructor_family.cpp", "src/object_registration.cpp",
        "src/manager_facade.cpp", "src/manager_init.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
    ]
    compile_and_run("metadata-utf16-buffer", originals.encode("utf-8"),
                    harness, [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
