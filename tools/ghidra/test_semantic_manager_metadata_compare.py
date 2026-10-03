"""Compare the new full comparison body with reused CRT error algorithms."""
import argparse
import json
from pathlib import Path
from generate_manager_metadata_compare import ROOT, MANIFEST, reviewed
from semantic_batch import compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path.home() / "ownCloud/Git/LostOdysseyRecomp")
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-manager-metadata-compare-tests")
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest != reviewed(args.source_root):
        raise ValueError("comparison manifest changed")
    declarations = "void OriginalError(PPCContext&, std::uint8_t*);\nvoid OriginalInvalid(PPCContext&, std::uint8_t*);\n"
    bridges = "#define sub_82B7FD78(ctx, base) OriginalError(ctx, base)\n#define sub_82B7FEC0(ctx, base) OriginalInvalid(ctx, base)\n"
    originals = declarations + bridges + manifest["entries"][0]["translated_body"]
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/manager_metadata_compare_oracle.cpp").read_text(encoding="utf-8")
    compile_and_run("manager-metadata-compare", originals.encode(), harness,
        ["LostOdysseyRecompSemantics/src/" + name + ".cpp" for name in
            ("manager_metadata_compare", "allocation_failure", "crt_thread_data", "invalid_parameter")], args.output)


if __name__ == "__main__":
    main()
