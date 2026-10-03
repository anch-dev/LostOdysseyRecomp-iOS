"""Compare four CRT stream-state entries with fixed original PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_crt_stream_state import MANIFEST, ROOT, generate
from semantic_batch import compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-crt-stream-state-tests")
    args = parser.parse_args()
    manifest = generate(args.ppc_root)
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != manifest:
        raise ValueError("CRT stream-state manifest differs")
    prelude = b"""
void OriginalError(PPCContext&, std::uint8_t*);
void OriginalInvalid(PPCContext&, std::uint8_t*);
void OriginalRawAllocate(PPCContext&, std::uint8_t*);
void OriginalNative(PPCContext&, std::uint8_t*);
void OriginalIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
void OriginalUnexpected(PPCContext&, std::uint8_t*);
#define sub_82B7FD78(ctx, base) OriginalError(ctx, base)
#define sub_82B7FEC0(ctx, base) OriginalInvalid(ctx, base)
#define sub_823ACBD0(ctx, base) OriginalRawAllocate(ctx, base)
#define sub_822CA180(ctx, base) OriginalUnexpected(ctx, base)
#define __imp__RtlInitializeCriticalSection(ctx, base) OriginalNative(ctx, base)
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(target) OriginalIndirect(ctx, base, target)
"""
    originals = b"\n".join([prelude] + [
        entry["translated_body"].encode("utf-8") for entry in manifest["entries"]])
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/crt_stream_state_oracle.cpp")\
        .read_bytes()
    names = ("crt_stream_state", "raw_allocation", "allocation_failure",
             "crt_thread_data", "invalid_parameter", "memory_services")
    result = compile_and_run("crt-stream-state", originals, harness,
        ["LostOdysseyRecompSemantics/src/" + name + ".cpp" for name in names],
        args.output)
    print(result)


if __name__ == "__main__":
    main()
