"""Compare three new option-matching entries against nested original PPC."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from generate_metadata_option_match import MANIFEST, ROOT, generate
from semantic_batch import compile_and_run

HELPERS = (("savegprlr_25", 5599, 25), ("restgprlr_25", 6179, 25),
           ("savegprlr_29", 5691, 29), ("restgprlr_29", 6279, 29))


def helper_body(ppc_root: Path, name: str, line: int, first: int) -> bytes:
    lines = (ppc_root / "ppc_recomp.175.cpp").read_bytes().splitlines(keepends=True)
    start = line - 1
    if lines[start].strip() != f"PPC_FUNC_IMPL(__imp____{name}) {{".encode():
        raise ValueError(f"ABI helper moved: {name}")
    end = next(index for index in range(start + 1, start + 32)
               if lines[index].strip() == b"}")
    body = b"".join(lines[start:end + 1])
    restoring = name.startswith("rest")
    operation = "ld" if restoring else "std"
    expected = [f"{operation} r{reg},-{8 * (33 - reg)}(r1)"
                for reg in range(first, 32)]
    expected += (["lwz r12,-8(r1)", "mtlr r12", "blr"] if restoring
                 else ["stw r12,-8(r1)", "blr"])
    observed = [x.decode("ascii") for x in re.findall(
        rb"^\s*//\s*(.*?)\s*$", body, re.M)]
    if observed != expected:
        raise ValueError(f"ABI helper body changed: {name}")
    return body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-metadata-option-match-tests")
    args = parser.parse_args()
    manifest = generate(args.ppc_root)
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != manifest:
        raise ValueError("fixed option matching manifest changed")
    lower = json.loads((ROOT / "LostOdysseyRecompSemantics/registered_metadata_string_families.json")
                       .read_text(encoding="utf-8"))
    length_body = next(x["translated_body"] for x in lower["entries"]
                       if x["address"] == "82296830")
    prelude = b"""
PPC_FUNC(__savegprlr_25); PPC_FUNC(__restgprlr_25);
PPC_FUNC(__savegprlr_29); PPC_FUNC(__restgprlr_29);
void OriginalError(PPCContext&, std::uint8_t*);
void OriginalInvalid(PPCContext&, std::uint8_t*);
#define sub_82B7FD78(ctx, base) OriginalError(ctx, base)
#define sub_82B7FEC0(ctx, base) OriginalInvalid(ctx, base)
#define sub_82296830(ctx, base) __imp__sub_82296830(ctx, base)
#define sub_82296858(ctx, base) __imp__sub_82296858(ctx, base)
#define sub_82297390(ctx, base) __imp__sub_82297390(ctx, base)
"""
    wrappers = b"""
PPC_FUNC(__savegprlr_25) { __imp____savegprlr_25(ctx, base); }
PPC_FUNC(__restgprlr_25) { __imp____restgprlr_25(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
"""
    originals = b"\n".join([prelude] + [helper_body(args.ppc_root, *spec)
        for spec in HELPERS] + [wrappers, length_body.encode("utf-8")] +
        [entry["translated_body"].encode("utf-8")
         for entry in manifest["entries"]])
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/metadata_option_match_oracle.cpp")\
        .read_bytes()
    sources = ["metadata_option_match", "registered_metadata_string",
        "registered_metadata_words", "registered_metadata_composed",
        "registered_constructor_family", "registered_callback_family",
        "registered_getter_family", "registered_inline_constructor",
        "object_registration", "object_startup", "manager_facade", "manager_init",
        "allocation_array", "memory_move", "allocation_failure",
        "crt_thread_data", "invalid_parameter"]
    result = compile_and_run("metadata-option-match", originals, harness,
        ["LostOdysseyRecompSemantics/src/" + x + ".cpp" for x in sources],
        args.output)
    print(result)


if __name__ == "__main__":
    main()
