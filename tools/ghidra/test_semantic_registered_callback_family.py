"""Compare the 708 recovered registration callbacks with cached PPC bodies."""

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run


def number(value):
    return int(value, 16) if value else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-registered-callback-tests")
    args = parser.parse_args()
    entries = json.loads((ROOT / "LostOdysseyRecompSemantics/registered_callback_families.json")
                         .read_text(encoding="utf-8"))["entries"]
    candidates = {"0x" + item["address"]: item for item in json.loads(
        (ROOT / "out/function-inventory/registered-callback-candidates.json")
        .read_text(encoding="utf-8"))}
    constructors = {number(item["address"]): item for item in json.loads(
        (ROOT / "LostOdysseyRecompSemantics/registered_constructor_families.json")
        .read_text(encoding="utf-8"))["entries"]}
    if len(entries) != 708 or len(candidates) != 708 or [number(x["address"]) for x in entries] != \
            sorted(set(number(x["address"]) for x in entries)):
        raise ValueError("callback inventory changed")
    table, bodies, direct_targets = [], [], set()
    for entry in entries:
        address = entry["address"]
        original = candidates[address]
        if not re.fullmatch(r"0x[0-9A-F]{8}", address) or \
                original["generated_ppc_path"] != entry["source"] or \
                original["line"] != entry["source_line"] or \
                not original["body"].startswith(f"PPC_FUNC_IMPL(__imp__sub_{address[2:]})"):
            raise ValueError(f"callback source mapping changed: {address}")
        if entry["shape_id"] not in range(11) or entry["frame_size"] not in (112, 128):
            raise ValueError(f"callback shape changed: {address}")
        def singleton(side):
            getter = number(entry[side]["getter"])
            return number(constructors[getter]["singleton_address"]) if getter in constructors else 0
        table.append("    {" + ", ".join([
            f"{address}u", f"{number(entry['self_global']):#x}u",
            f"{number(entry['parent']['getter']):#x}u",
            f"{singleton('parent'):#x}u",
            f"{number(entry['parent']['global']):#x}u",
            f"{number(entry['parent']['registration']):#x}u",
            f"{number(entry['meta']['getter']):#x}u",
            f"{singleton('meta'):#x}u",
            f"{number(entry['meta']['global']):#x}u",
            f"{entry['frame_size']}u", f"{entry['shape_id']}u",
            f"__imp__sub_{address[2:]}" ]) + "},")
        bodies.append(original["body"])
        direct_targets.update(int(x, 16) for x in re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\)", original["body"]))
    helper = (ROOT / "out/function-inventory/central-82403200.cpp").read_text(encoding="utf-8")
    if not helper.startswith("PPC_FUNC_IMPL(__imp__sub_82403200)"):
        raise ValueError("shared metadata PPC cache changed")
    direct_targets.update(int(x, 16) for x in re.findall(
        r"\bsub_([0-9A-F]{8})\(ctx, base\)", helper))
    direct_targets.discard(0x82403200)
    declarations = [
        "void OriginalDirectCall(PPCContext&, uint8_t*, uint32_t);",
        "void OriginalIndirectCall(PPCContext&, uint8_t*, uint32_t);",
        "PPC_FUNC(__savegprlr_28); PPC_FUNC(__restgprlr_28);",
        "PPC_FUNC(__savegprlr_29); PPC_FUNC(__restgprlr_29);",
        "#undef PPC_CALL_INDIRECT_FUNC",
        "#define PPC_CALL_INDIRECT_FUNC(x) OriginalIndirectCall(ctx, base, x)",
        *(f"PPC_FUNC(sub_{target:08X});" for target in sorted(direct_targets | {0x82403200})),
    ]
    stubs = [
        *(f"PPC_FUNC(sub_{target:08X}) {{ OriginalDirectCall(ctx, base, 0x{target:08X}u); }}"
          for target in sorted(direct_targets)),
        "PPC_FUNC(sub_82403200) { __imp__sub_82403200(ctx, base); }",
    ]
    original_cpp = "\n".join([*declarations, *bodies, helper, *stubs]).encode("utf-8")
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/registered_callback_family_oracle.cpp")
    template = harness.read_text(encoding="utf-8")
    if template.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("callback oracle table marker changed")
    compile_and_run(
        "registered-callback-family", original_cpp,
        template.replace("/* ENTRY_TABLE */", "\n".join(table)),
        ["LostOdysseyRecompSemantics/src/registered_callback_family.cpp",
         "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
         "LostOdysseyRecompSemantics/src/object_registration.cpp",
         "LostOdysseyRecompSemantics/src/object_startup.cpp",
         "LostOdysseyRecompSemantics/src/manager_facade.cpp",
         "LostOdysseyRecompSemantics/src/manager_init.cpp",
         "LostOdysseyRecompSemantics/src/allocation_array.cpp",
         "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
