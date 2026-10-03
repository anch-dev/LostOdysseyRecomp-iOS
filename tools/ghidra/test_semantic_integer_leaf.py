"""Strictly check and batch-compare the two reviewed integer-leaf groups."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from semantic_batch import compile_and_run, extract_originals  # noqa: E402

MANIFEST = ROOT / "LostOdysseyRecompSemantics/integer_leaf_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/integer_leaf_oracle.cpp"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/integer_leaf.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-integer-leaf-tests"


def strict_bodies(entries: list[dict], ppc_root: Path) -> None:
    source_lines: dict[str, list[str]] = {}
    for entry in entries:
        address = entry["address"]
        sequence = entry["instruction_sequence"]
        effects = entry["generated_effect_statements"]
        if sequence[-1] != "blr" or len(effects) != len(sequence) - 1:
            raise ValueError(f"unreviewed instruction/effect layout: {address}")
        filename = Path(entry["generated_ppc_path"]).name
        if filename not in source_lines:
            source_lines[filename] = (ppc_root / filename).read_text(
                encoding="utf-8").splitlines()
        expected = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
                    "\tPPC_FUNC_PROLOGUE();"]
        for instruction, effect in zip(sequence[:-1], effects):
            expected.extend((f"\t// {instruction}", f"\t{effect}"))
        expected.extend(("\t// blr", "\treturn;", "}"))
        start = entry["line"] - 1
        actual = source_lines[filename][start:start + len(expected)]
        if [line.rstrip(" \t") for line in actual] != expected:
            raise ValueError(f"generated body changed: {address} {filename}:{entry['line']}")


def harness(entries: list[dict]) -> bytes:
    lines = [
        "struct IntegerLeafEntry { unsigned address; void (*original)(PPCContext&, uint8_t*); };",
        "static const IntegerLeafEntry kIntegerLeafEntries[] = {",
    ]
    for entry in entries:
        address = entry["address"]
        lines.append(f"    {{0x{address}u, &__imp__sub_{address}}},")
    lines.append("};")
    return ("\n".join(lines) + "\n").encode() + ORACLE.read_bytes()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1:
        raise ValueError("unsupported integer-leaf map")
    entries = data["entries"]
    addresses = [entry["address"] for entry in entries]
    groups = {group: sum(e["group"] == group for e in entries) for group in
              ("constant_return_with_all_gpr_effects",
               "straight_line_integer_leaf_with_all_gpr_effects")}
    if addresses != sorted(set(addresses)) or groups != {
        "constant_return_with_all_gpr_effects": 224,
        "straight_line_integer_leaf_with_all_gpr_effects": 72,
    } or sum(e["already_in_recovery_manifest"] for e in entries) != 1:
        raise ValueError("integer-leaf membership changed")
    strict_bodies(entries, args.ppc_root)
    original_cpp = extract_originals(entries, args.ppc_root)
    result = compile_and_run("integer-leaf", original_cpp, harness(entries),
                             [SOURCE], args.output)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
