"""Compare focused RPNavi chains with exact cached original PPC functions."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_vtable_family import raw_bodies
from semantic_batch import ROOT, compile_and_run, extract_originals


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-navigation-tests")
    args = parser.parse_args()
    manifest = json.loads((ROOT /
        "LostOdysseyRecompSemantics/instance_navigation_initializer_families.json").read_text())
    if manifest["entry_count"] != 16 or len(manifest["entries"]) != 16 or \
            len({entry["address"] for entry in manifest["entries"]}) != 16 or \
            len(manifest["chains"]) != 8:
        raise ValueError("navigation manifest changed")
    all_bodies = raw_bodies(ROOT /
        "out/function-inventory/registered-instance-originals.cpp.gz")
    callee_cache = json.loads((ROOT /
        "out/function-inventory/instance-navigation-callees.json").read_text())
    callees = {entry["address"]: entry["body"].encode("ascii")
               for entry in callee_cache["entries"]}
    by_address = {entry["address"]: entry for entry in manifest["chains"]}
    sampled = ("82B55028", "82B550E8", "82B56728")
    table = []
    original = []
    for address in sampled:
        entry = by_address[address]
        original.append("\n".join(all_bodies[address]).encode("ascii"))
        table.append(f"    {{0x{address.lower()}u, {entry['derived_vtable'].lower()}u, "
                     f"__imp__sub_{address}}},")
    for address in ("82B54518", "82B54980", "82B548F8", "82B55FF8"):
        original.append(callees[address])
        table.append(f"    {{0x{address.lower()}u, 0u, __imp__sub_{address}}},")
    # These five targets have independently recovered constant-field semantics;
    # compare against their real original bodies, not invented callback stubs.
    pointer = json.loads((ROOT /
        "LostOdysseyRecompSemantics/pointer_fields_families.json").read_text())
    lower_addresses = {by_address[address]["lower"] for address in sampled}
    lower_addresses.add(by_address["82B550E8"]["lower"])
    lower_addresses.add(by_address["82B55028"]["lower"])
    lower_addresses.add(by_address["82B56728"]["lower"])
    # The nested 82B54980 calls 82B548F8, whose lower is 824D83A0.
    lower_addresses.add(by_address["82B550A8"]["lower"])
    lowers = [entry for entry in pointer["entries"]
              if entry["address"] in lower_addresses and entry["address"] != "82B548F8"]
    if len(lowers) != 2:
        raise ValueError("focused original lower dependency set changed")
    source = Path.home() / "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc"
    original.append(extract_originals([
        {"address": entry["address"], "generated_ppc_path": entry["source"],
         "line": entry["source_line"],
         "instruction_sequence": entry["instruction_sequence"]}
        for entry in lowers], source))
    included = sorted(set(sampled) |
                      {"82B54518", "82B54980", "82B548F8", "82B55FF8"} |
                      {entry["address"] for entry in lowers})
    declarations = "\n".join(
        f'extern "C" PPC_FUNC(__imp__sub_{address});\n'
        f'PPC_FUNC(sub_{address}) {{ __imp__sub_{address}(ctx, base); }}'
        for address in included)
    harness = (ROOT /
        "LostOdysseyRecompSemantics/tests/instance_navigation_initializer_family_oracle.cpp").read_text()
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("navigation oracle marker changed")
    compile_and_run("instance-navigation-initializer",
        declarations.encode("ascii") + b"\n" + b"\n".join(original),
        harness.replace("/* ENTRY_TABLE */", "\n".join(table)),
        ["LostOdysseyRecompSemantics/src/instance_navigation_initializer_family.cpp",
         "LostOdysseyRecompSemantics/src/pointer_fields.cpp"], args.output)


if __name__ == "__main__":
    main()
