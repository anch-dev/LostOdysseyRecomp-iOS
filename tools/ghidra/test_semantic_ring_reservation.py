"""Compare only finite ordinary-RAM paths; hardware synchronization is separate."""

import argparse
import json
from pathlib import Path

from generate_ring_reservation import reviewed
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-ring-reservation-tests")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/ring_reservation_families.json")
        .read_text(encoding="utf-8"))
    if manifest != reviewed(args.source_root / "LostOdysseyRecompLib/ppc"):
        raise ValueError("reviewed ring reservation body changed")
    compile_and_run("ring-reservation", manifest["entries"][0]["translated_body"].encode(),
        (ROOT / "LostOdysseyRecompSemantics/tests/ring_reservation_oracle.cpp")
            .read_text(encoding="utf-8"),
        ["LostOdysseyRecompSemantics/src/ring_reservation.cpp"], args.output)


if __name__ == "__main__":
    main()
