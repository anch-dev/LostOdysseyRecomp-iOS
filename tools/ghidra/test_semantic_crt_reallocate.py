"""Compare the fixed CRT realloc body with the accepted direct PPC models."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_crt_reallocate import generate
from semantic_batch import ROOT, compile_and_run
from test_semantic_metadata_name_index import HELPERS, helper_body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-crt-reallocate-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "crt_reallocate_families.json").read_text()) != manifest:
        raise ValueError("CRT realloc manifest changed")
    prelude = b"""
#define __savegprlr_27(ctx, base) __imp____savegprlr_27(ctx, base)
#define __restgprlr_27(ctx, base) __imp____restgprlr_27(ctx, base)
void OriginalRaw(PPCContext&, std::uint8_t*);
void OriginalFree(PPCContext&, std::uint8_t*);
void OriginalGetHeap(PPCContext&, std::uint8_t*);
void OriginalHeap(PPCContext&, std::uint8_t*);
void OriginalNewHandler(PPCContext&, std::uint8_t*);
void OriginalErrorAddress(PPCContext&, std::uint8_t*);
void OriginalLastError(PPCContext&, std::uint8_t*);
void OriginalTranslate(PPCContext&, std::uint8_t*);
#define sub_823ACBD0(ctx, base) OriginalRaw(ctx, base)
#define sub_823ADDC0(ctx, base) OriginalFree(ctx, base)
#define sub_823ACC98(ctx, base) OriginalGetHeap(ctx, base)
#define sub_827CCF80(ctx, base) OriginalHeap(ctx, base)
#define sub_82B7FE68(ctx, base) OriginalNewHandler(ctx, base)
#define sub_82B7FD78(ctx, base) OriginalErrorAddress(ctx, base)
#define sub_822CA100(ctx, base) OriginalLastError(ctx, base)
#define sub_82B7FD10(ctx, base) OriginalTranslate(ctx, base)
"""
    originals = b"\n".join([prelude,
        helper_body(args.ppc_root, *HELPERS[0]),
        helper_body(args.ppc_root, *HELPERS[1]),
        manifest["entries"][0]["translated_body"].encode()])
    names = ("crt_reallocate", "heap_reallocate", "heap_block_resize",
        "heap_lock_exit", "heap_segment", "heap_allocate", "heap_free",
        "heap", "memory_fill", "memory_move", "memory_services",
        "crt_allocation", "crt_last_error", "allocation_failure",
        "raw_allocation")
    compile_and_run("crt-reallocate", originals,
        (semantics / "tests/crt_reallocate_oracle.cpp").read_bytes(),
        [semantics / f"src/{name}.cpp" for name in names], args.output)


if __name__ == "__main__":
    main()
