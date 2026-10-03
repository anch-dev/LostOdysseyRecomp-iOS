"""Compare recovered registration callbacks with cached PPC bodies."""

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
    parser.add_argument("--scope", choices=("original", "dependency", "composition", "next", "special"),
                        default="original")
    args = parser.parse_args()
    manifest = ("registered_dependency_registration_families.json"
                if args.scope in ("dependency", "next") else "registered_callback_families.json")
    entries = json.loads((ROOT / "LostOdysseyRecompSemantics" / manifest)
                         .read_text(encoding="utf-8"))["entries"]
    if args.scope == "special":
        entries = []
        for address, filename, self_global, parent_global, registration, shape in (
            ("827CE088", "registered-next-dependencies.json", "0x83247210",
             "0x833180DC", "0x824B0B60", 0),
            ("824084F0", "registered-secondary-candidate.json", "0x83315F7C",
             "0x83315F60", "0x82403200", 1),
        ):
            cached = json.loads((ROOT / "out/function-inventory" / filename)
                                .read_text(encoding="utf-8"))
            if isinstance(cached, list):
                cached = next(item for item in cached if item["address"] == address)
            entries.append({
                "address": "0x" + address, "source": cached["generated_ppc_path"],
                "source_line": cached["line"], "shape_id": shape,
                "frame_size": 128, "self_global": self_global,
                "parent": {"getter": None, "global": parent_global,
                           "registration": registration},
                "meta": {"getter": None, "global": "0x83315F60"},
            })
        entries.sort(key=lambda entry: number(entry["address"]))
    next_addresses = {"0x824C9190", "0x826D8380", "0x825DB460",
                      "0x8255EE88", "0x8256C408", "0x824B0B60"}
    if args.scope == "special":
        records = [json.loads((ROOT / "out/function-inventory/registered-secondary-candidate.json")
                              .read_text(encoding="utf-8")),
                   next(item for item in json.loads((ROOT / "out/function-inventory/registered-next-dependencies.json")
                                                    .read_text(encoding="utf-8"))
                        if item["address"] == "827CE088")]
    elif args.scope == "next":
        entries = [entry for entry in entries if entry["address"] in next_addresses]
    elif args.scope == "dependency":
        entries = [entry for entry in entries if entry["address"] not in next_addresses]
    if args.scope == "composition":
        representative = {"0x824073E8", "0x8249C688", "0x8259C4A8",
                          "0x8271FEF0"}
        entries = [entry for entry in entries if entry["address"] in representative]
        if len(entries) != len(representative):
            raise ValueError("composition representatives changed")
    catalog = ("registered-dependency-candidates.json" if args.scope == "dependency"
               else "registered-callback-candidates.json")
    if args.scope == "next":
        records = json.loads((ROOT / "out/function-inventory/registered-next-dependencies.json")
                             .read_text(encoding="utf-8"))
        records.append(json.loads((ROOT / "out/function-inventory/registered-final-dependency.json")
                                  .read_text(encoding="utf-8")))
    elif args.scope != "special":
        records = json.loads((ROOT / "out/function-inventory" / catalog)
                             .read_text(encoding="utf-8"))
    candidates = {"0x" + item["address"]: item for item in records}
    constructors = {number(item["address"]): item for item in json.loads(
        (ROOT / "LostOdysseyRecompSemantics/registered_constructor_families.json")
        .read_text(encoding="utf-8"))["entries"]}
    expected = {"original": 708, "dependency": 51,
                "composition": 4, "next": 6, "special": 2}[args.scope]
    catalog_size = {"original": 708, "dependency": 104,
                    "composition": 708, "next": 14, "special": 2}[args.scope]
    if len(entries) != expected or len(candidates) != catalog_size or \
            [number(x["address"]) for x in entries] != \
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
        if args.scope in ("dependency", "next") and (not entry["ready_gate_before_primary_store"] or
                                            not entry["ready_capture_from_self_field"]):
            raise ValueError(f"dependency ready ordering changed: {address}")
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
    gate = None
    if args.scope == "special":
        gate = json.loads((ROOT / "out/function-inventory/registered-gate-candidate.json")
                          .read_text(encoding="utf-8"))["body"]
        if not gate.startswith("PPC_FUNC_IMPL(__imp__sub_824009F8)"):
            raise ValueError("ready gate PPC cache changed")
        direct_targets.discard(0x824009F8)
    declarations = [
        "void OriginalDirectCall(PPCContext&, uint8_t*, uint32_t);",
        "void OriginalIndirectCall(PPCContext&, uint8_t*, uint32_t);",
        "PPC_FUNC(__savegprlr_28); PPC_FUNC(__restgprlr_28);",
        "PPC_FUNC(__savegprlr_29); PPC_FUNC(__restgprlr_29);",
        "#undef PPC_CALL_INDIRECT_FUNC",
        "#define PPC_CALL_INDIRECT_FUNC(x) OriginalIndirectCall(ctx, base, x)",
        *(f"PPC_FUNC(sub_{target:08X});" for target in sorted(direct_targets | {0x82403200} |
                                                      ({0x824009F8} if gate else set()))),
    ]
    stubs = [
        *(f"PPC_FUNC(sub_{target:08X}) {{ OriginalDirectCall(ctx, base, 0x{target:08X}u); }}"
          for target in sorted(direct_targets)),
        "PPC_FUNC(sub_82403200) { __imp__sub_82403200(ctx, base); }",
        *( ["PPC_FUNC(sub_824009F8) { __imp__sub_824009F8(ctx, base); }"] if gate else [] ),
    ]
    original_cpp = "\n".join([*declarations, *bodies, helper,
                               *([gate] if gate else []), *stubs]).encode("utf-8")
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/registered_callback_family_oracle.cpp")
    template = harness.read_text(encoding="utf-8")
    if template.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("callback oracle table marker changed")
    compile_and_run(
        {"original": "registered-callback-family",
         "dependency": "registered-dependency-callback-family",
         "composition": "registered-callback-composition",
         "next": "registered-next-callback",
         "special": "registered-special-callback"}[args.scope], original_cpp,
        template.replace("/* ENTRY_TABLE */", "\n".join(table)),
        ["LostOdysseyRecompSemantics/src/registered_callback_family.cpp",
         "LostOdysseyRecompSemantics/src/registered_getter_family.cpp",
         "LostOdysseyRecompSemantics/src/registered_inline_constructor.cpp",
         "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
         "LostOdysseyRecompSemantics/src/object_registration.cpp",
         "LostOdysseyRecompSemantics/src/object_startup.cpp",
         "LostOdysseyRecompSemantics/src/manager_facade.cpp",
         "LostOdysseyRecompSemantics/src/manager_init.cpp",
         "LostOdysseyRecompSemantics/src/allocation_array.cpp",
         "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
