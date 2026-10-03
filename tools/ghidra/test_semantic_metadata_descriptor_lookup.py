"""Compare two new descriptor algorithms with exact original PPC bodies."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from generate_metadata_descriptor_lookup import generate
from semantic_batch import ROOT, compile_and_run

HELPERS = (
    ("savegprlr_24", 5571, [
        "std r24,-72(r1)", "std r25,-64(r1)", "std r26,-56(r1)",
        "std r27,-48(r1)", "std r28,-40(r1)", "std r29,-32(r1)",
        "std r30,-24(r1)", "std r31,-16(r1)", "stw r12,-8(r1)", "blr"]),
    ("restgprlr_24", 6149, [
        "ld r24,-72(r1)", "ld r25,-64(r1)", "ld r26,-56(r1)",
        "ld r27,-48(r1)", "ld r28,-40(r1)", "ld r29,-32(r1)",
        "ld r30,-24(r1)", "ld r31,-16(r1)", "lwz r12,-8(r1)",
        "mtlr r12", "blr"]),
    ("savegprlr_27", 5649, [
        "std r27,-48(r1)", "std r28,-40(r1)", "std r29,-32(r1)",
        "std r30,-24(r1)", "std r31,-16(r1)", "stw r12,-8(r1)", "blr"]),
    ("restgprlr_27", 6233, [
        "ld r27,-48(r1)", "ld r28,-40(r1)", "ld r29,-32(r1)",
        "ld r30,-24(r1)", "ld r31,-16(r1)", "lwz r12,-8(r1)",
        "mtlr r12", "blr"]),
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
                        "worktrees/LostOdysseyRecomp/semantic-metadata-descriptor-lookup-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
                        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "metadata_descriptor_lookup_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("descriptor manifest changed")
    declarations = """
PPC_FUNC(__savegprlr_24);
PPC_FUNC(__restgprlr_24);
PPC_FUNC(__savegprlr_27);
PPC_FUNC(__restgprlr_27);
void OriginalNameLookup(PPCContext&, std::uint8_t*);
#define sub_82296D30(ctx, base) OriginalNameLookup(ctx, base)
#define sub_8229D160(ctx, base) __imp__sub_8229D160(ctx, base)
"""
    helper_bodies = b"\n".join(helper_body(args.ppc_root, *helper)
                              for helper in HELPERS)
    wrappers = """
PPC_FUNC(__savegprlr_24) { __imp____savegprlr_24(ctx, base); }
PPC_FUNC(__restgprlr_24) { __imp____restgprlr_24(ctx, base); }
PPC_FUNC(__savegprlr_27) { __imp____savegprlr_27(ctx, base); }
PPC_FUNC(__restgprlr_27) { __imp____restgprlr_27(ctx, base); }
"""
    originals = (declarations.encode("utf-8") + b"\n" + helper_bodies +
                 wrappers.encode("utf-8") + b"\n" + b"\n\n".join(
                     entry["translated_body"].encode("utf-8")
                     for entry in manifest["entries"]))
    harness = (semantics / "tests/metadata_descriptor_lookup_oracle.cpp")\
        .read_text(encoding="utf-8")
    sources = [
        "src/metadata_descriptor_lookup.cpp", "src/metadata_name_lookup.cpp",
        "src/metadata_name_registry.cpp", "src/metadata_name_index.cpp",
        "src/metadata_name_record.cpp", "src/manager_metadata_parsing.cpp",
        "src/manager_metadata_compare.cpp", "src/manager_object_registration.cpp",
        "src/registered_metadata_string.cpp", "src/registered_metadata_composed.cpp",
        "src/registered_metadata_words.cpp", "src/registered_constructor_family.cpp",
        "src/registered_callback_family.cpp", "src/registered_getter_family.cpp",
        "src/registered_inline_constructor.cpp", "src/object_registration.cpp",
        "src/object_startup.cpp", "src/manager_facade.cpp", "src/manager_init.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
        "src/allocation_failure.cpp", "src/crt_thread_data.cpp",
        "src/invalid_parameter.cpp",
    ]
    compile_and_run("metadata-descriptor-lookup", originals,
                    harness, [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
