"""Compare the 845 new constructor wrappers with cached generated PPC bodies."""

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-registered-constructor-tests")
    args = parser.parse_args()

    implementation = json.loads(
        (ROOT / "LostOdysseyRecompSemantics/registered_constructor_families.json")
        .read_text(encoding="utf-8"))
    catalog = json.loads(
        (ROOT / "out/function-inventory/registered-constructor-candidates.json")
        .read_text(encoding="utf-8"))
    entries = implementation["entries"]
    addresses = [entry["address"] for entry in entries]
    candidates = {entry["address"]: entry for entry in catalog}
    if len(entries) != 845 or addresses != sorted(set(addresses)) or \
            "82410B90" in addresses or len(candidates) != 847:
        raise ValueError("registered-constructor entry inventory changed")
    kinds = {"constructor": "Constructor",
             "constructor_stack_scratch": "Scratch",
             "lazy_singleton": "Singleton"}
    callbacks = set()
    bodies = []
    table = []
    for entry in entries:
        address = entry["address"]
        candidate = candidates[address]
        kind = entry["kind"]
        callback = entry["post_callback"]
        if not re.fullmatch(r"[0-9A-F]{8}", address) or kind not in kinds or \
                candidate["source"] != entry["source"] or \
                candidate["source_line"] != entry["source_line"] or \
                not candidate["body"].startswith(f"PPC_FUNC_IMPL(__imp__sub_{address})") or \
                callback != (candidate["calls"][-1] if kind == "lazy_singleton" else None):
            raise ValueError(f"constructor source mapping changed: {address}")
        expected_calls = ["82486C88", "82410A28"]
        if callback:
            expected_calls.append(callback)
            callbacks.add(callback)
        if candidate["calls"] != expected_calls:
            raise ValueError(f"constructor call graph changed: {address}")
        # These are the only outgoing stores; preserve their observed order.
        stack_stores = []
        for instruction in candidate["instructions"]:
            match = re.fullmatch(r"(?:stw|std) r[0-9]+,([0-9]+)\(r1\)", instruction)
            if match and int(match[1]) in (80, 92, 100, 108):
                stack_stores.append(int(match[1]))
        if stack_stores != entry["store_order"]:
            raise ValueError(f"constructor outgoing store order changed: {address}")
        if kind == "constructor_stack_scratch":
            if entry["frame_size"] != 144 or entry["scratch_offset"] != 112:
                raise ValueError(f"constructor scratch mapping changed: {address}")
        elif entry["frame_size"] != 128 or entry["scratch_offset"] is not None:
            raise ValueError(f"constructor frame mapping changed: {address}")
        singleton = int(entry["singleton_address"], 16) if entry["singleton_address"] else 0
        callback_address = int(callback, 16) if callback else 0
        table.append(
            f"    {{0x{address}u, Variant::{kinds[kind]}, 0x{singleton:08x}u, "
            f"0x{callback_address:08x}u, {entry['frame_size']}u, __imp__sub_{address}}},")
        bodies.append(candidate["body"])
    if len(callbacks) != 708 or set(addresses) & callbacks:
        raise ValueError("constructor callback set changed")

    declarations = [
        "std::uint64_t AllocateLower(PPCContext&);",
        "std::uint64_t InitializeLower(PPCContext&);",
        "std::uint64_t RegistrationCallback(PPCContext&, std::uint32_t);",
        "PPC_FUNC(sub_82486C88);",
        "PPC_FUNC(sub_82410A28);",
        *(f"PPC_FUNC(sub_{callback});" for callback in sorted(callbacks)),
    ]
    stubs = [
        "PPC_FUNC(sub_82486C88) { (void)base; ctx.r3.u64 = AllocateLower(ctx); }",
        "PPC_FUNC(sub_82410A28) { (void)base; ctx.r3.u64 = InitializeLower(ctx); }",
        *(f"PPC_FUNC(sub_{callback}) {{ (void)base; "
          f"ctx.r3.u64 = RegistrationCallback(ctx, 0x{callback}u); }}"
          for callback in sorted(callbacks)),
    ]
    original = "\n".join([*declarations, *bodies, *stubs]).encode("utf-8")
    harness = (ROOT /
               "LostOdysseyRecompSemantics/tests/registered_constructor_family_oracle.cpp"
               ).read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("constructor oracle table marker changed")
    compile_and_run(
        "registered-constructor-family", original,
        harness.replace("/* ENTRY_TABLE */", "\n".join(table)),
        ["LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
         "LostOdysseyRecompSemantics/src/object_registration.cpp",
         "LostOdysseyRecompSemantics/src/manager_facade.cpp",
         "LostOdysseyRecompSemantics/src/manager_init.cpp",
         "LostOdysseyRecompSemantics/src/allocation_array.cpp",
         "LostOdysseyRecompSemantics/src/memory_move.cpp"],
        args.output)


if __name__ == "__main__":
    main()
