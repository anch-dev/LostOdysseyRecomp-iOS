"""Compare six closed metadata parser entries with their exact cached PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_manager_metadata_parsing import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-manager-metadata-parsing-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "manager_metadata_parsing_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("manager metadata parsing manifest changed")
    targets = ["82296830", "8232D318", "822974B0", "82376FA8",
               "82376F98", "82B7D688", "82B7D3E0", "82B7FD78",
               "82B7FEC0"]
    prefix = """
void OriginalDirectCall(PPCContext&, std::uint8_t*, std::uint32_t);
void SaveGprs(PPCContext&, std::uint8_t*, unsigned);
void RestoreGprs(PPCContext&, std::uint8_t*, unsigned);
#define __savegprlr_23(ctx, base) SaveGprs(ctx, base, 23)
#define __restgprlr_23(ctx, base) RestoreGprs(ctx, base, 23)
#define __savegprlr_27(ctx, base) SaveGprs(ctx, base, 27)
#define __restgprlr_27(ctx, base) RestoreGprs(ctx, base, 27)
""" + "\n".join(f"#define sub_{target}(ctx, base) "
                   f"OriginalDirectCall(ctx, base, 0x{target.lower()}u)"
                   for target in targets)
    originals = prefix + "\n" + "\n".join(
        entry["translated_body"] for entry in manifest["entries"])
    harness = (semantics / "tests/manager_metadata_parsing_oracle.cpp")\
        .read_text(encoding="utf-8")
    sources = [
        "src/manager_metadata_parsing.cpp", "src/manager_object_registration.cpp",
        "src/registered_metadata_string.cpp", "src/registered_metadata_words.cpp",
        "src/registered_constructor_family.cpp", "src/registered_callback_family.cpp",
        "src/registered_getter_family.cpp", "src/registered_inline_constructor.cpp",
        "src/object_registration.cpp", "src/object_startup.cpp",
        "src/manager_facade.cpp", "src/manager_init.cpp",
        "src/allocation_array.cpp", "src/memory_move.cpp",
        "src/allocation_failure.cpp", "src/crt_thread_data.cpp",
        "src/invalid_parameter.cpp",
    ]
    compile_and_run("manager-metadata-parsing", originals.encode("utf-8"),
                    harness, [semantics / source for source in sources], args.output)


if __name__ == "__main__":
    main()
