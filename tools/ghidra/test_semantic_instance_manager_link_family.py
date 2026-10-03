"""Compare the two closed manager-link foundations with original PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_manager_link_family import originals
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-instance-manager-link-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = json.loads((semantics / "instance_manager_link_families.json")
                          .read_text(encoding="utf-8"))
    actual = originals(args.ppc_root)
    if manifest["entry_count"] != 2 or manifest["entries"] != actual or \
            manifest["excluded_open_chain"] != ["827010D0", "82700D10"]:
        raise ValueError("manager-link exact original manifest changed")
    declarations = "PPC_FUNC(sub_82700FD8);\n"
    bodies = declarations + "\n".join(entry["translated_body"] for entry in actual)
    harness = (semantics / "tests/instance_manager_link_oracle.cpp")
    compile_and_run("instance-manager-link", bodies.encode("utf-8"),
        harness.read_bytes(),
        ["LostOdysseyRecompSemantics/src/instance_manager_link_family.cpp"],
        args.output)


if __name__ == "__main__":
    main()
