"""Compare 825E7600 with its exact PPC body and accepted lookup model."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_metadata_name_dispatch import original
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
        "worktrees/LostOdysseyRecomp/semantic-metadata-name-dispatch-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = json.loads((semantics /
        "metadata_name_dispatch_families.json").read_text(encoding="utf-8"))
    if manifest["entry_count"] != 1 or manifest["entries"] != [
            original(args.ppc_root)]:
        raise ValueError("metadata-name dispatch exact manifest changed")
    prelude = b"""
void DispatchOriginalLookup(PPCContext&, std::uint8_t*);
void DispatchOriginalIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
PPC_FUNC(__savegprlr_29);
PPC_FUNC(__restgprlr_29);
#define sub_82296D30(ctx, base) DispatchOriginalLookup(ctx, base)
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) DispatchOriginalIndirect(ctx, base, address)
"""
    helpers = b"\n".join(helper_body(args.ppc_root, *helper)
                         for helper in HELPERS)
    wrappers = b"""
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
"""
    originals = b"\n".join([prelude, helpers, wrappers,
        manifest["entries"][0]["translated_body"].encode("utf-8")])
    harness = (semantics / "tests/metadata_name_dispatch_oracle.cpp")\
        .read_bytes()
    sources = [
        "src/metadata_name_dispatch.cpp", "src/metadata_name_lookup.cpp",
        "src/metadata_name_registry.cpp", "src/metadata_name_index.cpp",
        "src/metadata_name_record.cpp", "src/manager_metadata_parsing.cpp",
        "src/manager_metadata_compare.cpp", "src/manager_object_registration.cpp",
        "src/registered_metadata_string.cpp",
        "src/registered_metadata_composed.cpp", "src/registered_metadata_words.cpp",
        "src/registered_constructor_family.cpp", "src/registered_callback_family.cpp",
        "src/registered_getter_family.cpp", "src/registered_inline_constructor.cpp",
        "src/object_registration.cpp", "src/object_startup.cpp",
        "src/manager_facade.cpp", "src/manager_init.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
        "src/allocation_failure.cpp", "src/crt_thread_data.cpp",
        "src/invalid_parameter.cpp",
    ]
    compile_and_run("metadata-name-dispatch", originals, harness,
        [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
