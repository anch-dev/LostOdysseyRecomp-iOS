"""Compare both complete descriptor-name PPC bodies with accepted callees."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_metadata_descriptor_names import generate
from semantic_batch import ROOT, compile_and_run

HELPERS = (("savegprlr_28", 5671, 28),
           ("restgprlr_28", 6257, 28),
           ("savegprlr_29", 5691, 29),
           ("restgprlr_29", 6279, 29))


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
        "worktrees/LostOdysseyRecomp/semantic-metadata-descriptor-names-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate(args.ppc_root)
    if json.loads((semantics / "metadata_descriptor_names_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("descriptor-name full-body manifest changed")
    prelude = b"""
PPC_FUNC(__savegprlr_28);
PPC_FUNC(__restgprlr_28);
PPC_FUNC(__savegprlr_29);
PPC_FUNC(__restgprlr_29);
PPC_FUNC(sub_8232CED8);
PPC_FUNC(sub_8229C8B0);
PPC_FUNC(sub_8232D418);
PPC_FUNC(sub_82298938);
PPC_FUNC(sub_8232D378);
PPC_FUNC(sub_8229F5E0);
PPC_FUNC(sub_822A9668);
PPC_FUNC(sub_823AC8E0);
"""
    helpers = b"\n".join(helper_body(args.ppc_root, *helper)
                         for helper in HELPERS)
    wrappers = b"""
PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
"""
    originals = b"\n".join([prelude, helpers, wrappers] + [
        entry["translated_body"].encode("utf-8")
        for entry in manifest["entries"]])
    sources = [
        "src/metadata_descriptor_names.cpp", "src/metadata_utf16_slice.cpp",
        "src/metadata_utf16_buffer.cpp", "src/manager_object_registration.cpp",
        "src/string_property_initializer.cpp",
        "src/registered_metadata_string.cpp",
        "src/registered_metadata_words.cpp",
        "src/registered_metadata_composed.cpp",
        "src/registered_constructor_family.cpp",
        "src/registered_callback_family.cpp",
        "src/registered_getter_family.cpp",
        "src/registered_inline_constructor.cpp",
        "src/object_registration.cpp", "src/object_startup.cpp",
        "src/manager_facade.cpp", "src/manager_init.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
    ]
    compile_and_run("metadata-descriptor-names", originals,
        (semantics / "tests/metadata_descriptor_names_oracle.cpp").read_bytes(),
        [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
