"""Compare 823F3298 with its exact PPC body and manager initializer."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_metadata_manager_dispatch import original
from semantic_batch import ROOT, compile_and_run


HELPERS = (
    ("savegprlr_29", 5691, ["std r29,-32(r1)", "std r30,-24(r1)",
        "std r31,-16(r1)", "stw r12,-8(r1)", "blr"]),
    ("restgprlr_29", 6279, ["ld r29,-32(r1)", "ld r30,-24(r1)",
        "ld r31,-16(r1)", "lwz r12,-8(r1)", "mtlr r12", "blr"]),
)


def helper_body(ppc_root: Path, name: str, line: int,
                expected: list[str]) -> bytes:
    lines = (ppc_root / "ppc_recomp.175.cpp").read_bytes().splitlines(
        keepends=True)
    start = line - 1
    if lines[start].strip() != f"PPC_FUNC_IMPL(__imp____{name}) {{".encode():
        raise ValueError(f"ABI helper source line changed: {name}")
    end = next(index for index in range(start + 1, start + 32)
               if lines[index].strip() == b"}")
    body = b"".join(lines[start:end + 1])
    instructions = [value.decode("ascii") for value in re.findall(
        rb"^\s*//\s*(.*?)\s*$", body, re.M)]
    if instructions != expected:
        raise ValueError(f"ABI helper body changed: {name}")
    return body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-metadata-manager-dispatch-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = json.loads((semantics /
        "metadata_manager_dispatch_families.json").read_text(encoding="utf-8"))
    if manifest["entry_count"] != 1 or manifest["entries"] != [
            original(args.ppc_root)]:
        raise ValueError("manager-dispatch exact manifest changed")
    prelude = b"""
void OriginalInitializeManager(PPCContext&, std::uint8_t*);
void OriginalDispatchMethod(PPCContext&, std::uint8_t*, std::uint32_t);
PPC_FUNC(__savegprlr_29);
PPC_FUNC(__restgprlr_29);
#define sub_827C5F38(ctx, base) OriginalInitializeManager(ctx, base)
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) OriginalDispatchMethod(ctx, base, address)
"""
    helpers = b"\n".join(helper_body(args.ppc_root, *helper)
                         for helper in HELPERS)
    wrappers = b"""
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
"""
    bodies = b"\n".join([prelude, helpers, wrappers,
        manifest["entries"][0]["translated_body"].encode("utf-8")])
    harness = (semantics / "tests/metadata_manager_dispatch_oracle.cpp")\
        .read_bytes()
    compile_and_run("metadata-manager-dispatch", bodies, harness, [
        semantics / "src/metadata_manager_dispatch.cpp",
        semantics / "src/manager_init.cpp",
    ], args.output)


if __name__ == "__main__":
    main()
