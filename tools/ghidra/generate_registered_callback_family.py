"""Extract reviewed callback parameters from the cached exact PPC bodies.

No PPC source scan is performed. Each candidate must match one of the eleven
reviewed instruction skeletons and the semantic call/global constraints below.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CANDIDATES = ROOT / "out/function-inventory/registered-callback-candidates.json"
SHAPES = ROOT / "out/function-inventory/registered-callback-shapes.json"
CONSTRUCTORS = ROOT / "LostOdysseyRecompSemantics/registered_constructor_families.json"
OUTPUT = ROOT / "LostOdysseyRecompSemantics/registered_callback_families.json"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/registered_callback_family.cpp"
START = "    // BEGIN GENERATED REGISTERED CALLBACK PARAMETERS"
END = "    // END GENERATED REGISTERED CALLBACK PARAMETERS"

# Indices name semantic sites, not machine-code positions in the C++ runtime.
# All 708 exact bodies are checked against their full reviewed skeleton first.
LAYOUT = {
    0: (5, ("direct", 3, 8), ("lazy", 17, 21, 23), 28, 32, 34, 39, 43),
    1: (14, ("lazy", 4, 9, 11, 17), ("lazy", 26, 30, 32), 37, 41, 43, 48, 52),
    2: (15, ("lazy", 6, 10, 12, 18), ("lazy", 24, 28, 30), 35, 39, 41, 46, 50),
    3: (15, ("lazy", 6, 10, 12, 18), ("lazy", 25, 29, 31), 36, 41, 43, 48, 52),
    4: (7, ("direct", 5, 10), ("direct", 16), 20, 25, 27, 32, 36),
    5: (7, ("direct", 5, 10), ("lazy", 17, 22, 24), 29, 34, 36, 41, 45),
    6: (15, ("lazy", 6, 10, 12, 18), ("lazy", 25, 29, 31), 36, 40, 42, 47, 51),
    7: (15, ("lazy", 6, 10, 12, 18), ("direct", 24), 28, 32, 34, 39, 43),
    8: (5, ("direct", 3, 8), ("lazy", 17, 21, 23), 28, 32, 34, 39, None),
    9: (15, ("lazy", 6, 10, 12, None), ("lazy", 29, 33, 35), 40, 44, 46, 51, None),
    10: (15, ("lazy", 6, 10, 12, 18), ("lazy", 25, 30, 32), 37, 41, 43, 48, 52),
}

FIELD_STORES = {
    0: (10, 13, 27, 38), 1: (19, 22, 36, 47),
    2: (20, 23, 34, 45), 3: (20, 23, 35, 47),
    4: (12, 15, 19, 31), 5: (12, 15, 28, 40),
    6: (20, 23, 35, 46), 7: (20, 23, 27, 38),
    8: (10, 13, 27, 38), 9: (28, 39, 50),
    10: (20, 23, 36, 47),
}


def require(value: bool, message: str) -> None:
    if not value:
        raise ValueError(message)


def normalized(instruction: str) -> str:
    instruction = re.sub(r"0x[0-9a-fA-F]+", "ADDR", instruction)
    return re.sub(r"(?<![A-Za-z_0-9])-?[0-9]+(?![A-Za-z_0-9])", "N", instruction)


def control_flow(address: int, instructions: list[str]) -> list[tuple[int, int]]:
    edges = []
    for index, instruction in enumerate(instructions):
        match = re.fullmatch(r"b(?:eq|ne)?(?: cr6)?,0x([0-9a-fA-F]+)", instruction)
        if match is None:
            continue
        target = int(match.group(1), 16)
        # Terminal branches to the shared GPR restore thunk are not local CFG.
        destination = (target - address) // 4 if address <= target < address + 4 * len(instructions) else target
        edges.append((index, destination))
    return edges


def field_offset(instruction: str) -> int:
    match = re.fullmatch(r"stw r\d+,(-?\d+)\(r\d+\)", instruction)
    require(match is not None, f"expected word store: {instruction}")
    return int(match.group(1))


def call(instructions: list[str], index: int) -> int:
    match = re.fullmatch(r"bl 0x([0-9a-fA-F]+)", instructions[index])
    require(match is not None, f"expected direct call at {index}")
    return int(match.group(1), 16)


def full_address(instructions: list[str], index: int) -> int:
    match = re.fullmatch(r"lwz r\d+,(-?\d+)\((r\d+)\)", instructions[index])
    require(match is not None, f"expected global load at {index}")
    displacement, base = int(match.group(1)), match.group(2)
    previous = next((entry for entry in reversed(instructions[:index])
                     if entry.startswith(f"lis {base},")), None)
    require(previous is not None, f"missing global base at {index}")
    hi = int(previous.rsplit(",", 1)[1])
    return ((hi << 16) + displacement) & 0xffffffff


def register_at_call(instructions: list[str], index: int) -> int:
    registers: dict[str, int | None] = {"r3": None}
    for instruction in instructions[:index]:
        if match := re.fullmatch(r"lis (r\d+),(-?\d+)", instruction):
            registers[match.group(1)] = (int(match.group(2)) << 16) & 0xffffffffffffffff
        elif match := re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", instruction):
            source = registers.get(match.group(2))
            registers[match.group(1)] = None if source is None else (
                source + int(match.group(3))) & 0xffffffffffffffff
        elif match := re.fullmatch(r"mr (r\d+),(r\d+)", instruction):
            registers[match.group(1)] = registers.get(match.group(2))
        elif instruction.startswith("bl "):
            registers["r3"] = None
    value = registers.get("r3")
    require(value is not None, f"expected constant r3 at call {index}")
    return value


def hex32(value: int) -> str:
    return f"0x{value:08X}"


def hex64(value: int) -> str:
    return f"0x{value:016X}"


def decode_source(instructions: list[str], layout: tuple,
                  known: dict[int, dict], role: str) -> dict:
    if layout[0] == "direct":
        getter = call(instructions, layout[1])
        if len(layout) == 3:
            require(call(instructions, layout[2]) == getter,
                    f"{role} repeated getter changed")
        return {"mode": "direct", "getter": hex32(getter),
                "global": None, "constructor": None,
                "registration": None, "descriptor": None}

    global_address = full_address(instructions, layout[1])
    constructor = call(instructions, layout[2])
    registration = call(instructions, layout[3])
    require(constructor in known, f"unknown {role} constructor {constructor:08X}")
    known_spec = known[constructor]
    if known_spec["kind"] == "lazy_singleton":
        require(int(known_spec["singleton_address"], 16) == global_address,
                f"{role} singleton address mismatch")
        require(int(known_spec["post_callback"], 16) == registration,
                f"{role} registration callback mismatch")
    else:
        require(known_spec["kind"] == "constructor", f"{role} constructor kind changed")
    descriptor = register_at_call(instructions, layout[2])
    getter = call(instructions, layout[4]) if len(layout) == 5 and layout[4] is not None else None
    return {"mode": "lazy", "getter": hex32(getter) if getter else None,
            "global": hex32(global_address),
            "constructor": hex32(constructor),
            "registration": hex32(registration),
            "descriptor": hex64(descriptor)}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidates", type=Path, default=CANDIDATES)
    parser.add_argument("--shapes", type=Path, default=SHAPES)
    args = parser.parse_args()
    candidates = json.loads(args.candidates.read_text(encoding="utf-8"))
    shapes = json.loads(args.shapes.read_text(encoding="utf-8"))
    constructors = {int(entry["address"], 16): entry for entry in
                    json.loads(CONSTRUCTORS.read_text(encoding="utf-8"))["entries"]}
    by_address = {int(address, 16): group for group, shape in enumerate(shapes)
                  for address in shape["addresses"]}
    require(len(candidates) == len(by_address) == 708, "candidate count mismatch")
    first_by_shape = {group: next(candidate for candidate in candidates
                        if int(candidate["address"], 16) in
                        {int(item, 16) for item in shape["addresses"]})
                      for group, shape in enumerate(shapes)}
    flow_by_shape = {group: control_flow(int(candidate["address"], 16),
                     candidate["instructions"])
                     for group, candidate in first_by_shape.items()}
    entries = []
    for candidate in candidates:
        address = int(candidate["address"], 16)
        require(address in by_address, f"unclassified {address:08X}")
        group = by_address[address]
        instructions = candidate["instructions"]
        signature = shapes[group]["instruction_shape"]
        require([normalized(item) for item in instructions] == signature,
                f"instruction skeleton changed: {address:08X}")
        require(control_flow(address, instructions) == flow_by_shape[group],
                f"control flow changed: {address:08X}")
        stores = FIELD_STORES[group]
        offsets = (60, 196, 52) if group == 9 else (60, 60, 196, 52)
        require(tuple(field_offset(instructions[index]) for index in stores) == offsets,
                f"link field offsets changed: {address:08X}")
        require(sum(instruction.startswith("lwz ") and ",124(" in instruction
                    for instruction in instructions) == 1,
                f"ready vtable slot changed: {address:08X}")
        self_index, parent_layout, meta_layout, primary_index, \
            primary_ctor_index, graph_index, gate_index, ready_index = LAYOUT[group]
        frame = 128 if group in (8, 9) else 112
        require(instructions[2 if group != 4 and group != 5 else 4] ==
                f"stwu r1,-{frame}(r1)", f"frame mismatch: {address:08X}")
        parent = decode_source(instructions, parent_layout, constructors, "parent")
        meta = decode_source(instructions, meta_layout, constructors, "meta")
        primary = full_address(instructions, primary_index)
        gate = full_address(instructions, gate_index)
        require(primary == 0x83315f9c and gate == 0x83315ed8,
                f"primary or gate changed: {address:08X}")
        require(call(instructions, primary_ctor_index) == 0x82410b90,
                f"primary constructor changed: {address:08X}")
        require(call(instructions, graph_index) == 0x82410c48,
                f"graph registrar changed: {address:08X}")
        if ready_index is not None:
            require(call(instructions, ready_index) == 0x82406b00,
                    f"ready getter changed: {address:08X}")
        self_global = full_address(instructions, self_index)
        entry = {"address": hex32(address), "source": candidate["generated_ppc_path"],
                 "source_line": candidate["line"], "shape_id": group,
                 "frame_size": frame, "self_global": hex32(self_global),
                 "parent": parent, "meta": meta,
                 "primary_global": hex32(primary), "ready_gate": hex32(gate),
                 "primary_inline": group in (8, 9)}
        entries.append(entry)
    entries.sort(key=lambda entry: entry["address"])
    require(len({entry["address"] for entry in entries}) == 708,
            "duplicate callback address")
    require(sum(entry["primary_inline"] for entry in entries) == 3,
            "special inline count changed")
    payload = {"schema_version": 1, "source": str(CANDIDATES.relative_to(ROOT)).replace("\\", "/"),
               "family": "registered_callback", "entry_count": 708,
               "entries": entries}
    output = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"
    if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != output:
        OUTPUT.write_text(output, encoding="utf-8")
    if SOURCE.exists():
        source = SOURCE.read_text(encoding="utf-8")
        require(source.count(START) == source.count(END) == 1,
                "generated C++ markers missing")
        rows = []
        for entry in entries:
            parent, meta = entry["parent"], entry["meta"]
            fields = [entry["address"], str(entry["shape_id"]),
                      str(entry["frame_size"]), entry["self_global"]]
            for item in (parent, meta):
                source_fields = ["true" if item["mode"] == "lazy" else "false",
                                 item["getter"] or "0", item["global"] or "0",
                                 item["constructor"] or "0", item["registration"] or "0",
                                 item["descriptor"] or "0"]
                fields.append("{" + ", ".join(source_fields) + "}")
            fields.append("true" if entry["primary_inline"] else "false")
            rows.append("    {" + ", ".join(fields) + "},")
        first = source.index(START) + len(START)
        last = source.index(END)
        updated = source[:first] + "\n" + "\n".join(rows) + "\n    " + source[last:]
        if updated != source:
            SOURCE.write_text(updated, encoding="utf-8")
    print(f"classified {len(entries)} registered callbacks (705 generic, 3 inline)")


if __name__ == "__main__":
    main()
