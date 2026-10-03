"""Compare finite component defaults with exact cached PPC bodies."""

from __future__ import annotations

import argparse
import gzip
import json
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run


SAMPLES = ("825C2C28", "825E0788", "826B6478")
IMAGE_BASE = 0x82000000
F13_SOURCE = 0x8218958C
F0_SOURCE = 0x82000E50


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-component-tests")
    parser.add_argument("--image", type=Path, default=Path.home() /
                        "ownCloud/Git/LostOdysseyRecomp/"
                        "LostOdysseyRecompLib/private/image_disc1.bin")
    args = parser.parse_args()

    manifest = json.loads((ROOT /
        "LostOdysseyRecompSemantics/instance_component_initializer_families.json")
        .read_text(encoding="utf-8"))
    entries = manifest["entries"]
    if manifest["entry_count"] != 13 or \
            len({entry["address"] for entry in entries}) != 13 or \
            manifest["float_constant_addresses"] != [
                f"0x{F13_SOURCE:08X}", f"0x{F0_SOURCE:08X}"]:
        raise ValueError("component initializer manifest changed")
    by_address = {entry["address"]: entry for entry in entries}

    with args.image.open("rb") as image:
        image.seek(F13_SOURCE - IMAGE_BASE)
        f13_bytes = image.read(4)
        image.seek(F0_SOURCE - IMAGE_BASE)
        f0_bytes = image.read(4)
    if len(f13_bytes) != 4 or len(f0_bytes) != 4:
        raise ValueError("private image does not contain both FP constants")
    f13_word = int.from_bytes(f13_bytes, "big")
    f0_word = int.from_bytes(f0_bytes, "big")
    print(f"image constants {F13_SOURCE:08X}={f13_word:08X} "
          f"{F0_SOURCE:08X}={f0_word:08X}")

    raw = gzip.decompress((ROOT /
        "out/function-inventory/registered-instance-originals.cpp.gz").read_bytes())
    parts = re.split(rb"(?=PPC_FUNC_IMPL\(__imp__sub_[0-9A-F]{8}\) \{)", raw)
    bodies = {}
    for part in parts:
        if not part.strip():
            continue
        match = re.match(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{", part)
        if match is None:
            raise ValueError("cached PPC body boundary changed")
        bodies[match.group(1).decode("ascii")] = part.strip()
    table = []
    originals = []
    for address in SAMPLES:
        entry = by_address[address]
        body = bodies[address]
        if not body.startswith(f"PPC_FUNC_IMPL(__imp__sub_{address}) {{".encode()):
            raise ValueError(f"original body mismatch: {address}")
        originals.append(body)
        table.append(f"    {{0x{address.lower()}u, {entry['vtable'].lower()}u, "
                     f"__imp__sub_{address}}},")
    harness = (ROOT /
        "LostOdysseyRecompSemantics/tests/instance_component_initializer_family_oracle.cpp"
        ).read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1 or \
            harness.count("/* IMAGE_WORDS */") != 1:
        raise ValueError("component oracle markers changed")
    harness = harness.replace("/* ENTRY_TABLE */", "\n".join(table)).replace(
        "/* IMAGE_WORDS */",
        f"constexpr std::uint32_t ImageF13Word = 0x{f13_word:08x}u;\n"
        f"constexpr std::uint32_t ImageF0Word = 0x{f0_word:08x}u;")
    compile_and_run(
        "instance-component-initializer", b"\n".join(originals),
        harness,
        ["LostOdysseyRecompSemantics/src/instance_component_initializer_family.cpp"],
        args.output)


if __name__ == "__main__":
    main()
