"""Compare seven finite loaded-single extensions with cached original PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_fp_extension_family import (
    MANIFEST, ROOT, SOURCE, expected_manifest, source_with_table,
)
from generate_instance_vtable_family import raw_bodies
from semantic_batch import compile_and_run


F13_SOURCE = 0x8218958C
F0_SOURCE = 0x82000E50
IMAGE_BASE = 0x82000000


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-fp-extension-tests")
    parser.add_argument("--image", type=Path, default=Path.home() /
                        "ownCloud/Git/LostOdysseyRecomp/"
                        "LostOdysseyRecompLib/private/image_disc1.bin")
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest != expected_manifest() or SOURCE.read_text(encoding="utf-8") != \
            source_with_table():
        raise ValueError("FP extension source/manifest differs from cached PPC")
    entries = manifest["entries"]
    if len(entries) != 7 or manifest["float_sources"] != [
            f"0x{F13_SOURCE:08X}", f"0x{F0_SOURCE:08X}"]:
        raise ValueError("FP extension inventory/sources changed")
    with args.image.open("rb") as image:
        image.seek(F13_SOURCE - IMAGE_BASE)
        f13_bytes = image.read(4)
        image.seek(F0_SOURCE - IMAGE_BASE)
        f0_bytes = image.read(4)
    if len(f13_bytes) != 4 or len(f0_bytes) != 4:
        raise ValueError("private image source words unavailable")
    f13_word = int.from_bytes(f13_bytes, "big")
    f0_word = int.from_bytes(f0_bytes, "big")
    print(f"image source words {F13_SOURCE:08X}={f13_word:08X} "
          f"{F0_SOURCE:08X}={f0_word:08X}")

    bodies = raw_bodies(ROOT / "out/function-inventory/registered-instance-originals.cpp.gz")
    original = "\n\n".join("\n".join(bodies[entry["address"]])
                           for entry in entries)
    harness_path = ROOT / "LostOdysseyRecompSemantics/tests/instance_fp_extension_oracle.cpp"
    harness = harness_path.read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1 or \
            harness.count("/* IMAGE_WORDS */") != 1:
        raise ValueError("FP extension oracle markers changed")
    table = "\n".join(f"    {{0x{entry['address'].lower()}u, "
                      f"__imp__sub_{entry['address']}}}," for entry in entries)
    harness = harness.replace("/* ENTRY_TABLE */", table).replace(
        "/* IMAGE_WORDS */",
        f"constexpr std::uint32_t ImageF13Word = 0x{f13_word:08x}u;\n"
        f"constexpr std::uint32_t ImageF0Word = 0x{f0_word:08x}u;")
    compile_and_run("instance-fp-extension", original.encode("utf-8"),
                    harness,
                    ["LostOdysseyRecompSemantics/src/instance_fp_extension_family.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
