"""Compare only the new UTF16 search/prefix/replace entries."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from generate_metadata_utf16_search import generate
from generate_metadata_utf16_slice import generate as generate_slice
from test_semantic_metadata_utf16_slice import helper_body, HELPERS
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-metadata-utf16-search-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "metadata_utf16_search_families.json").read_text()) != manifest:
        raise ValueError("UTF16 search manifest changed")
    prelude = b"""
PPC_FUNC(__savegprlr_25); PPC_FUNC(__restgprlr_25);
PPC_FUNC(__savegprlr_27); PPC_FUNC(__restgprlr_27);
void OriginalDirectCall(PPCContext&, std::uint8_t*, std::uint32_t);
#define sub_8229D0E8(ctx, base) __imp__sub_8229D0E8(ctx, base)
#define sub_8232D240(ctx, base) __imp__sub_8232D240(ctx, base)
#define sub_822A06C0(ctx, base) OriginalDirectCall(ctx, base, 0x822a06c0u)
#define sub_8232D378(ctx, base) OriginalDirectCall(ctx, base, 0x8232d378u)
#define sub_82298938(ctx, base) OriginalDirectCall(ctx, base, 0x82298938u)
#define sub_82296830(ctx, base) OriginalDirectCall(ctx, base, 0x82296830u)
#define sub_8229F678(ctx, base) OriginalDirectCall(ctx, base, 0x8229f678u)
#define sub_8232D318(ctx, base) OriginalDirectCall(ctx, base, 0x8232d318u)
"""
    helpers = b"\n".join(helper_body(args.ppc_root, *spec)
        for spec in HELPERS if spec[2] in (25, 27))
    wrappers = b"""
PPC_FUNC(__savegprlr_25) { __imp____savegprlr_25(ctx, base); }
PPC_FUNC(__restgprlr_25) { __imp____restgprlr_25(ctx, base); }
PPC_FUNC(__savegprlr_27) { __imp____savegprlr_27(ctx, base); }
PPC_FUNC(__restgprlr_27) { __imp____restgprlr_27(ctx, base); }
"""
    constructor = next(e["translated_body"] for e in generate_slice()["entries"]
        if e["address"] == "8232D240").encode()
    originals = b"\n".join([prelude, helpers, wrappers, constructor] +
        [e["translated_body"].encode() for e in manifest["entries"]])
    sources = ["metadata_utf16_search", "metadata_utf16_slice", "metadata_utf16_buffer",
        "manager_object_registration", "string_property_initializer",
        "registered_metadata_string", "registered_metadata_words",
        "registered_metadata_composed", "registered_constructor_family",
        "registered_callback_family", "registered_getter_family",
        "registered_inline_constructor", "object_registration", "object_startup",
        "manager_facade", "manager_init", "allocation_array", "memory_move"]
    compile_and_run("metadata-utf16-search", originals,
        (semantics / "tests/metadata_utf16_search_oracle.cpp").read_bytes(),
        [semantics / f"src/{s}.cpp" for s in sources], args.output)

if __name__ == "__main__":
    main()
