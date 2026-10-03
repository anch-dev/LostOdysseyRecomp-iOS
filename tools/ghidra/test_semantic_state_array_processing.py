"""Compare three exact PPC state-array entries with recovered lower behavior."""

import argparse
import json
from pathlib import Path

from generate_state_array_processing import DEFAULT_PPC_ROOT, recover
from semantic_batch import ROOT, compile_and_run, extract_originals


def abi_helper_body(ppc_root, name, line):
    lines = (ppc_root / "ppc_recomp.175.cpp").read_text(
        encoding="utf-8").splitlines()
    if lines[line - 1] != f"PPC_FUNC_IMPL(__imp____{name}) {{":
        raise ValueError(f"recorded ABI helper location changed: {name}")
    end = line
    while end < len(lines) and lines[end] != "}":
        if lines[end].startswith("PPC_FUNC_IMPL("):
            raise ValueError(f"unterminated ABI helper: {name}")
        end += 1
    if end == len(lines):
        raise ValueError(f"unterminated ABI helper: {name}")
    return "\n".join(lines[line - 1:end + 1])


def lower_body(ppc_root, address, unit, line):
    entry = {"address": address, "line": line,
             "generated_ppc_path":
             f"LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp"}
    return extract_originals([entry], ppc_root).decode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path,
                        default=Path.home() / "worktrees/LostOdysseyRecomp"
                        / "semantic-state-array-tests")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics"
                           / "state_array_processing_families.json")
                          .read_text(encoding="utf-8"))
    if recover(args.ppc_root) != manifest or manifest["entry_count"] != 3:
        raise ValueError("tracked exact state-array PPC bodies changed")
    base = Path.home() / "worktrees/LostOdysseyRecomp"
    for path in [
        base / "semantic-allocation-array-tests/receipt-8229F678.json",
        base / "semantic-allocation-array-tests/receipt-82298AF8.json",
        base / "semantic-manager-init-tests/receipt-827C5F38.json",
    ]:
        if json.loads(path.read_text(encoding="utf-8"))["status"] != "passed":
            raise ValueError(f"lower comparison not passed: {path}")
    declarations = """
PPC_FUNC(__savegprlr_21); PPC_FUNC(__restgprlr_21);
PPC_FUNC(__savegprlr_27); PPC_FUNC(__restgprlr_27);
PPC_FUNC(__savegprlr_28); PPC_FUNC(__restgprlr_28);
PPC_FUNC(sub_823FDAB0); PPC_FUNC(sub_82407AD8);
PPC_FUNC(sub_8229F678); PPC_FUNC(sub_82298AF8);
PPC_FUNC(sub_82B7C470); PPC_FUNC(sub_82B7A0B0);
PPC_FUNC(sub_827C5F38);
void StateArrayIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) StateArrayIndirect(ctx, base, address)
"""
    originals = [declarations]
    for name, line in [("savegprlr_21", 5475), ("restgprlr_21", 6047),
                       ("savegprlr_27", 5649), ("restgprlr_27", 6233),
                       ("savegprlr_28", 5671), ("restgprlr_28", 6257)]:
        originals.append(abi_helper_body(args.ppc_root, name, line))
    for address, unit, line in [("82B7C470", 175, 12320),
                                ("8229F678", 0, 37441),
                                ("82298AF8", 0, 20894)]:
        originals.append(lower_body(args.ppc_root, address, unit, line))
    originals.extend(entry["translated_body"] for entry in manifest["entries"])
    harness = (ROOT / "LostOdysseyRecompSemantics/tests"
               / "state_array_processing_oracle.cpp").read_text(encoding="utf-8")
    compile_and_run("state-array", "\n".join(originals).encode("utf-8"),
                    harness, [
                        "LostOdysseyRecompSemantics/src/state_array_processing.cpp",
                        "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                        "LostOdysseyRecompSemantics/src/manager_init.cpp",
                        "LostOdysseyRecompSemantics/src/memory_move.cpp",
                    ], args.output)


if __name__ == "__main__":
    main()
