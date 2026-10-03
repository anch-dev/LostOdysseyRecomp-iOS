"""Compare descriptor dispatch/release bodies against bounded original PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_metadata_descriptor_lifecycle import generate
from semantic_batch import ROOT, compile_and_run

HELPERS = (("savegprlr_26", 5625, 26),
           ("restgprlr_26", 6207, 26),
           ("savegprlr_29", 5691, 29),
           ("restgprlr_29", 6279, 29))


def helper_body(ppc_root: Path, name: str, line: int, first: int) -> bytes:
    lines = (ppc_root / "ppc_recomp.175.cpp").read_bytes().splitlines(
        keepends=True)
    start = line - 1
    if lines[start].strip() != f"PPC_FUNC_IMPL(__imp____{name}) {{".encode():
        raise ValueError(f"ABI helper source moved: {name}")
    end = next(index for index in range(start + 1, start + 32)
               if lines[index].strip() == b"}")
    body = b"".join(lines[start:end + 1])
    restoring = name.startswith("rest")
    operation = "ld" if restoring else "std"
    expected = [f"{operation} r{reg},-{8 * (33 - reg)}(r1)"
                for reg in range(first, 32)]
    expected += (["lwz r12,-8(r1)", "mtlr r12", "blr"] if restoring
                 else ["stw r12,-8(r1)", "blr"])
    observed = [item.decode("ascii") for item in re.findall(
        rb"^\s*//\s*(.*?)\s*$", body, re.M)]
    if observed != expected:
        raise ValueError(f"ABI helper changed: {name}")
    return body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-metadata-descriptor-lifecycle-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate(args.ppc_root)
    if json.loads((semantics / "metadata_descriptor_lifecycle_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("descriptor lifecycle full-body manifest changed")
    prelude = b"""
PPC_FUNC(__savegprlr_26);
PPC_FUNC(__restgprlr_26);
PPC_FUNC(__savegprlr_29);
PPC_FUNC(__restgprlr_29);
PPC_FUNC(sub_823F3340);
PPC_FUNC(sub_82507598);
void OriginalVirtualCall(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(target) OriginalVirtualCall(ctx, base, target)
"""
    helpers = b"\n".join(helper_body(args.ppc_root, *helper)
                         for helper in HELPERS)
    wrappers = b"""
PPC_FUNC(__savegprlr_26) { __imp____savegprlr_26(ctx, base); }
PPC_FUNC(__restgprlr_26) { __imp____restgprlr_26(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
"""
    originals = b"\n".join([prelude, helpers, wrappers] + [
        entry["translated_body"].encode("utf-8")
        for entry in manifest["entries"]])
    compile_and_run("metadata-descriptor-lifecycle", originals,
        (semantics / "tests/metadata_descriptor_lifecycle_oracle.cpp")
        .read_bytes(), [semantics / source for source in [
            "src/metadata_descriptor_lifecycle.cpp",
            "src/manager_facade.cpp", "src/manager_init.cpp",
            "src/allocation_array.cpp", "src/memory_move.cpp"]], args.output)


if __name__ == "__main__":
    main()
