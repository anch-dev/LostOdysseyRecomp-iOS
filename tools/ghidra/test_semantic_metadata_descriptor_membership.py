"""Compare slot removal/change with original parent, child and ABI helpers."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_metadata_descriptor_membership import MANIFEST, generate
from test_semantic_metadata_descriptor_array import HELPERS as ARRAY_HELPERS
from test_semantic_metadata_descriptor_lookup import HELPERS as LOOKUP_HELPERS, helper_body
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-metadata-descriptor-membership-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
                        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    manifest = generate(args.ppc_root)
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != manifest:
        raise ValueError("membership manifest changed")
    helpers = [h for h in LOOKUP_HELPERS if h[0].endswith("_27")] + list(ARRAY_HELPERS)
    prelude = b"""
PPC_FUNC(__savegprlr_27);
PPC_FUNC(__restgprlr_27);
PPC_FUNC(__savegprlr_29);
PPC_FUNC(__restgprlr_29);
PPC_FUNC(sub_82298AF8);
PPC_FUNC(sub_825F41E8);
void OriginalMembershipCall(PPCContext&, std::uint8_t*, std::uint32_t);
#define sub_824080A8(ctx, base) __imp__sub_824080A8(ctx, base)
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) OriginalMembershipCall(ctx, base, address)
"""
    wrappers = b"""
PPC_FUNC(__savegprlr_27) { __imp____savegprlr_27(ctx, base); }
PPC_FUNC(__restgprlr_27) { __imp____restgprlr_27(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
"""
    originals = b"\n".join([prelude,
        b"\n".join(helper_body(args.ppc_root, *h) for h in helpers), wrappers,
        *[e["translated_body"].encode("utf-8") for e in manifest["entries"]]])
    semantics = ROOT / "LostOdysseyRecompSemantics"
    compile_and_run("metadata-descriptor-membership", originals,
        (semantics / "tests/metadata_descriptor_membership_oracle.cpp").read_bytes(),
        [semantics / "src/metadata_descriptor_membership.cpp",
         semantics / "src/allocation_array.cpp", semantics / "src/memory_move.cpp",
         semantics / "src/registered_metadata_words.cpp"], args.output)


if __name__ == "__main__":
    main()
