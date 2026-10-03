"""Compare two closed manager registration foundations with cached PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_manager_object_registration import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-manager-object-registration-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "manager_object_registration_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("manager object registration manifest changed")
    originals = """
void OriginalDirectCall(PPCContext&, std::uint8_t*, std::uint32_t);
#define sub_82408438(ctx, base) OriginalDirectCall(ctx, base, 0x82408438u)
#define sub_824084F0(ctx, base) OriginalDirectCall(ctx, base, 0x824084f0u)
""" + "\n".join(entry["translated_body"] for entry in manifest["entries"])
    harness = (semantics / "tests/manager_object_registration_oracle.cpp")\
        .read_text(encoding="utf-8")
    sources = [
        "src/manager_object_registration.cpp", "src/registered_constructor_family.cpp",
        "src/registered_callback_family.cpp", "src/registered_getter_family.cpp",
        "src/registered_inline_constructor.cpp", "src/object_registration.cpp",
        "src/object_startup.cpp", "src/manager_facade.cpp", "src/manager_init.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
    ]
    compile_and_run("manager-object-registration", originals.encode("utf-8"),
                    harness, [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
