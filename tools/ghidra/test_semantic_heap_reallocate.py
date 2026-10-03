"""Compare the complete 827CCF80 PPC body with both real new PPC children."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_heap_reallocate import MANIFEST, ROOT, generate
from generate_heap_block_resize import generate as resize_manifest
from generate_heap_lock_exit import generate as exit_manifest
from semantic_batch import compile_and_run


def abi_helper(ppc_root: Path, name: str, line: int) -> bytes:
    lines = (ppc_root / "ppc_recomp.175.cpp").read_bytes().splitlines(keepends=True)
    start = line - 1
    if lines[start].strip() != f"PPC_FUNC_IMPL(__imp____{name}) {{".encode():
        raise ValueError(f"ABI helper moved: {name}")
    end = next(i for i in range(start + 1, start + 40) if lines[i].strip() == b"}")
    return b"".join(lines[start:end + 1])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-heap-reallocate-tests")
    args = parser.parse_args()
    family = generate(args.ppc_root)
    resize = resize_manifest(args.ppc_root)
    exit = exit_manifest()
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != family:
        raise ValueError("heap reallocate manifest changed")
    for name, value in (("heap_block_resize", resize), ("heap_lock_exit", exit)):
        path = ROOT / f"LostOdysseyRecompSemantics/{name}_families.json"
        if json.loads(path.read_text(encoding="utf-8")) != value:
            raise ValueError(f"{name} manifest changed")

    prelude = b"""
PPC_FUNC(__savegprlr_19); PPC_FUNC(__restgprlr_19);
PPC_FUNC(__savegprlr_22); PPC_FUNC(__restgprlr_22);
extern "C" PPC_FUNC(__imp__sub_827CBCA8);
extern "C" PPC_FUNC(__imp__sub_827CD7BC);
void OriginalGetProcess(PPCContext&,std::uint8_t*);
void OriginalBugCheck(PPCContext&,std::uint8_t*);
void OriginalEnter(PPCContext&,std::uint8_t*);
void OriginalLeave(PPCContext&,std::uint8_t*);
void OriginalFreeVM(PPCContext&,std::uint8_t*);
void OriginalCompare(PPCContext&,std::uint8_t*);
void OriginalRaise(PPCContext&,std::uint8_t*);
void OriginalExtend(PPCContext&,std::uint8_t*);
void OriginalCoalesce(PPCContext&,std::uint8_t*);
void OriginalInsert(PPCContext&,std::uint8_t*);
void OriginalFill(PPCContext&,std::uint8_t*);
void OriginalMove(PPCContext&,std::uint8_t*);
void OriginalAllocate(PPCContext&,std::uint8_t*);
void OriginalFree(PPCContext&,std::uint8_t*);
#define sub_827CBCA8(ctx,base) __imp__sub_827CBCA8(ctx,base)
#define sub_827CD7BC(ctx,base) __imp__sub_827CD7BC(ctx,base)
#define sub_827CB778(ctx,base) OriginalExtend(ctx,base)
#define sub_823AE108(ctx,base) OriginalCoalesce(ctx,base)
#define sub_827CBA60(ctx,base) OriginalInsert(ctx,base)
#define sub_82B7BC40(ctx,base) OriginalFill(ctx,base)
#define sub_82B7C470(ctx,base) OriginalMove(ctx,base)
#define sub_823ACCB0(ctx,base) OriginalAllocate(ctx,base)
#define sub_823ADE28(ctx,base) OriginalFree(ctx,base)
#define __imp__KeGetCurrentProcessType(ctx,base) OriginalGetProcess(ctx,base)
#define __imp__KeBugCheckEx(ctx,base) OriginalBugCheck(ctx,base)
#define __imp__RtlEnterCriticalSection(ctx,base) OriginalEnter(ctx,base)
#define __imp__RtlLeaveCriticalSection(ctx,base) OriginalLeave(ctx,base)
#define __imp__NtFreeVirtualMemory(ctx,base) OriginalFreeVM(ctx,base)
#define __imp__RtlCompareMemoryUlong(ctx,base) OriginalCompare(ctx,base)
#define __imp__RtlRaiseException(ctx,base) OriginalRaise(ctx,base)
"""
    wrappers = b"""
PPC_FUNC(__savegprlr_19) { __imp____savegprlr_19(ctx,base); }
PPC_FUNC(__restgprlr_19) { __imp____restgprlr_19(ctx,base); }
PPC_FUNC(__savegprlr_22) { __imp____savegprlr_22(ctx,base); }
PPC_FUNC(__restgprlr_22) { __imp____restgprlr_22(ctx,base); }
"""
    helpers = [abi_helper(args.ppc_root, name, line) for name, line in (
        ("savegprlr_19", 5401), ("restgprlr_19", 5969),
        ("savegprlr_22", 5509), ("restgprlr_22", 6083))]
    originals = b"\n".join([prelude, *helpers, wrappers,
        resize["entries"][0]["translated_body"].encode(),
        exit["entries"][0]["translated_body"].encode(),
        family["entries"][0]["translated_body"].encode()])
    semantics = ROOT / "LostOdysseyRecompSemantics"
    sources = [semantics / f"src/{name}.cpp" for name in (
        "heap_reallocate", "heap_block_resize", "heap_lock_exit",
        "heap_segment", "heap",
        "heap_allocate", "heap_free", "memory_fill", "memory_move")]
    compile_and_run("heap-reallocate", originals,
        (semantics / "tests/heap_reallocate_oracle.cpp").read_bytes(),
        sources, args.output)


if __name__ == "__main__":
    main()
