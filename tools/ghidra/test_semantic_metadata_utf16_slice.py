"""Compare four UTF-16 slice/reverse bodies with original PPC composition."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from generate_metadata_utf16_slice import generate
from semantic_batch import ROOT, compile_and_run

HELPERS = (("savegprlr_25", 5599, 25),
           ("restgprlr_25", 6179, 25),
           ("savegprlr_27", 5649, 27),
           ("restgprlr_27", 6233, 27),
           ("savegprlr_28", 5671, 28),
           ("restgprlr_28", 6257, 28))


def helper_body(ppc_root: Path, name: str, line: int, first: int) -> bytes:
    lines = (ppc_root / "ppc_recomp.175.cpp").read_bytes().splitlines(
        keepends=True)
    start = line - 1
    if lines[start].strip() != f"PPC_FUNC_IMPL(__imp____{name}) {{".encode():
        raise ValueError(f"ABI helper location changed: {name}")
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
        raise ValueError(f"ABI helper instruction sequence changed: {name}")
    return body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-metadata-utf16-slice-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "metadata_utf16_slice_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("metadata UTF-16 slice manifest changed")
    prelude = b"""
PPC_FUNC(__savegprlr_25);
PPC_FUNC(__restgprlr_25);
PPC_FUNC(__savegprlr_27);
PPC_FUNC(__restgprlr_27);
PPC_FUNC(__savegprlr_28);
PPC_FUNC(__restgprlr_28);
void OriginalDirectCall(PPCContext&, std::uint8_t*, std::uint32_t);
void OriginalVirtualCall(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(target) OriginalVirtualCall(ctx, base, target)
#define sub_8232D240(ctx, base) __imp__sub_8232D240(ctx, base)
#define sub_8232D190(ctx, base) __imp__sub_8232D190(ctx, base)
#define sub_8232D040(ctx, base) __imp__sub_8232D040(ctx, base)
#define sub_8229F678(ctx, base) OriginalDirectCall(ctx, base, 0x8229f678u)
#define sub_8232D318(ctx, base) OriginalDirectCall(ctx, base, 0x8232d318u)
#define sub_8232D378(ctx, base) OriginalDirectCall(ctx, base, 0x8232d378u)
#define sub_827C5F38(ctx, base) OriginalDirectCall(ctx, base, 0x827c5f38u)
#define sub_82298AF8(ctx, base) OriginalDirectCall(ctx, base, 0x82298af8u)
#define sub_82298A98(ctx, base) OriginalDirectCall(ctx, base, 0x82298a98u)
"""
    helper_bodies = b"\n".join(helper_body(args.ppc_root, *spec)
                              for spec in HELPERS)
    wrappers = b"""
PPC_FUNC(__savegprlr_25) { __imp____savegprlr_25(ctx, base); }
PPC_FUNC(__restgprlr_25) { __imp____restgprlr_25(ctx, base); }
PPC_FUNC(__savegprlr_27) { __imp____savegprlr_27(ctx, base); }
PPC_FUNC(__restgprlr_27) { __imp____restgprlr_27(ctx, base); }
PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
"""
    originals = b"\n".join([prelude, helper_bodies, wrappers] + [
        entry["translated_body"].encode("utf-8")
        for entry in manifest["entries"]])
    harness = (semantics / "tests/metadata_utf16_slice_oracle.cpp")\
        .read_bytes()
    sources = [
        "src/metadata_utf16_slice.cpp", "src/metadata_utf16_buffer.cpp",
        "src/manager_object_registration.cpp", "src/string_property_initializer.cpp",
        "src/registered_metadata_string.cpp", "src/registered_metadata_words.cpp",
        "src/registered_metadata_composed.cpp", "src/registered_constructor_family.cpp",
        "src/registered_callback_family.cpp", "src/registered_getter_family.cpp",
        "src/registered_inline_constructor.cpp", "src/object_registration.cpp",
        "src/object_startup.cpp", "src/manager_facade.cpp", "src/manager_init.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
    ]
    compile_and_run("metadata-utf16-slice", originals, harness,
        [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
