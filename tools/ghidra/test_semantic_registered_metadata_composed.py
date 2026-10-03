"""Compare the closed metadata virtual thunk and UTF-16 helper with PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_registered_metadata_composed import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-metadata-composed-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "registered_metadata_composed_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("registered metadata composed manifest changed")
    originals = """
void MetadataIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) MetadataIndirect(ctx, base, address)
""" + "\n".join(entry["translated_body"] for entry in manifest["entries"])
    harness = (semantics / "tests/registered_metadata_composed_oracle.cpp")\
        .read_text(encoding="utf-8")
    compile_and_run("metadata-composed", originals.encode("utf-8"), harness,
                    ["LostOdysseyRecompSemantics/src/registered_metadata_composed.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
