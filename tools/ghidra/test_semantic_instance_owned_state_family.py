"""Compare eight new owned-state callers with original PPC compositions."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_owned_state_family import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-owned-state-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest, source = generate()
    if json.loads((semantics / "instance_owned_state_families.json")
                  .read_text(encoding="utf-8")) != manifest or \
            (semantics / "src/instance_owned_state_family.cpp")\
                    .read_text(encoding="utf-8") != source:
        raise ValueError("owned-state caller manifest/source differs from PPC")
    foundation_receipt = (Path.home() / "worktrees/LostOdysseyRecomp"
                          / "semantic-owned-state-tests"
                          / "owned-state-result.json")
    if json.loads(foundation_receipt.read_text(encoding="utf-8"))[
            "status"] != "passed":
        raise ValueError("owned-state foundation oracle not passed")
    foundation = json.loads((semantics / "owned_state_initializer_families.json")
                            .read_text(encoding="utf-8"))
    if {entry["address"] for entry in foundation["entries"]} != \
            {"823FA008", "82406A38"}:
        raise ValueError("owned-state foundation address set changed")
    declarations = "\n".join(f"PPC_FUNC({name});" for name in [
        "sub_82486C88", "sub_82406A38", "sub_823FA008",
        "sub_827134B8", "sub_82693E38",
    ]) + "\n" + """
void OwnedStateIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) OwnedStateIndirect(ctx, base, address)
"""
    originals = declarations + "\n" + "\n".join(
        [entry["translated_body"] for entry in foundation["entries"]] +
        ["\n".join(entry["body"]) for entry in manifest["entries"]])
    cases = []
    for entry in manifest["entries"]:
        address = entry["address"]
        cases.append(f"    {{0x{address.lower()}u, __imp__sub_{address}, "
                     "Mode::Ordinary},")
    for address in ("82693A10", "82693C38", "82693DD8", "82713418"):
        cases.append(f"    {{0x{address.lower()}u, __imp__sub_{address}, "
                     "Mode::Null},")
    for address in ("82693A10", "82693C38", "82693E38", "827134B8"):
        cases.append(f"    {{0x{address.lower()}u, __imp__sub_{address}, "
                     "Mode::SkipState},")
    cases += [
        "    {0x827134b8u, __imp__sub_827134B8, Mode::CallbackMutation},",
        "    {0x82713538u, __imp__sub_82713538, Mode::FrameAlias},",
        "    {0x82693c38u, __imp__sub_82693C38, Mode::AllocationFailure},",
        "    {0x82713538u, __imp__sub_82713538, Mode::HighSpCallback},",
    ]
    template = (semantics / "tests/instance_owned_state_oracle.cpp")\
        .read_text(encoding="utf-8")
    if template.count("/* CASE_TABLE */") != 1:
        raise ValueError("owned-state oracle case marker changed")
    sources = [
        "LostOdysseyRecompSemantics/src/instance_owned_state_family.cpp",
        "LostOdysseyRecompSemantics/src/owned_state_initializer.cpp",
        "LostOdysseyRecompSemantics/src/manager_facade.cpp",
        "LostOdysseyRecompSemantics/src/manager_init.cpp",
        "LostOdysseyRecompSemantics/src/allocation_array.cpp",
        "LostOdysseyRecompSemantics/src/memory_move.cpp",
    ]
    compile_and_run("instance-owned-state", originals.encode("utf-8"),
                    template.replace("/* CASE_TABLE */", "\n".join(cases)),
                    sources, args.output)


if __name__ == "__main__":
    main()
