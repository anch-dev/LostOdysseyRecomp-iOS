"""Compare 52 recovered singleton getters with cached generated PPC bodies."""

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-registered-getter-tests")
    parser.add_argument("--scope", choices=("original", "next"), default="original")
    args = parser.parse_args()
    entries = json.loads((ROOT / "LostOdysseyRecompSemantics/registered_getter_families.json")
                         .read_text(encoding="utf-8"))["entries"]
    next_addresses = {"822C9910", "8242FDD0", "82432DC0", "824605F8",
                      "82462980", "82463FE0"}
    if args.scope == "next":
        entries = [entry for entry in entries if entry["address"] in next_addresses]
        catalog = json.loads((ROOT / "out/function-inventory/registered-next-dependencies.json")
                             .read_text(encoding="utf-8"))
    else:
        entries = [entry for entry in entries if entry["address"] not in next_addresses]
        catalog = json.loads((ROOT / "out/function-inventory/registered-dependency-candidates.json")
                             .read_text(encoding="utf-8"))
    candidates = {item["address"]: item for item in catalog}
    constructors = {item["address"]: item for item in json.loads(
        (ROOT / "LostOdysseyRecompSemantics/registered_constructor_families.json")
        .read_text(encoding="utf-8"))["entries"]}
    expected = 6 if args.scope == "next" else 52
    if len(entries) != expected or [x["address"] for x in entries] != \
            sorted(set(x["address"] for x in entries)):
        raise ValueError("getter inventory changed")
    table, bodies, targets = [], [], set()
    for entry in entries:
        address = entry["address"]
        body = candidates[address]
        if not re.fullmatch(r"[0-9A-F]{8}", address) or \
                body["generated_ppc_path"] != entry["source"] or \
                body["line"] != entry["source_line"] or \
                not body["body"].startswith(f"PPC_FUNC_IMPL(__imp__sub_{address})") or \
                len(body["instructions"]) != 19 or entry["frame_size"] != 96 or \
                (entry["constructor"] != "827CE240" and
                 constructors[entry["constructor"]]["kind"] != "constructor"):
            raise ValueError(f"getter source mapping changed: {address}")
        called = re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\)", body["body"])
        if called != [entry["constructor"], entry["registration"]]:
            raise ValueError(f"getter call graph changed: {address}")
        table.append("    {" + ", ".join([
            f"0x{address}u", f"{entry['singleton_address']}u",
            f"0x{entry['constructor']}u", f"0x{entry['registration']}u",
            f"__imp__sub_{address}"]) + "},")
        bodies.append(body["body"])
        targets.update(called)
    original = "\n".join([
        "std::uint64_t ConstructorLower(PPCContext&, std::uint32_t);",
        "std::uint64_t RegistrationBoundary(PPCContext&, std::uint32_t);",
        *(f"PPC_FUNC(sub_{target});" for target in sorted(targets)),
        *bodies,
        *(f"PPC_FUNC(sub_{target}) {{ (void)base; ctx.r3.u64 = " +
          (f"ConstructorLower(ctx, 0x{target}u); }}" if target in constructors or
           target == "827CE240" else
           f"RegistrationBoundary(ctx, 0x{target}u); }}")
          for target in sorted(targets)),
    ]).encode("utf-8")
    template = (ROOT / "LostOdysseyRecompSemantics/tests/registered_getter_family_oracle.cpp")
    harness = template.read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("getter oracle table marker changed")
    compile_and_run(
        "registered-getter-next" if args.scope == "next" else "registered-getter-family", original,
        harness.replace("/* ENTRY_TABLE */", "\n".join(table)),
        ["LostOdysseyRecompSemantics/src/registered_getter_family.cpp",
         "LostOdysseyRecompSemantics/src/registered_inline_constructor.cpp",
         "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
         "LostOdysseyRecompSemantics/src/object_registration.cpp",
         "LostOdysseyRecompSemantics/src/manager_facade.cpp",
         "LostOdysseyRecompSemantics/src/manager_init.cpp",
         "LostOdysseyRecompSemantics/src/allocation_array.cpp",
         "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
