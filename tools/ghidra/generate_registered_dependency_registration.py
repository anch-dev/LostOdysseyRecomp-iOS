"""Classify the 51 cached registration dependencies and emit their parameters."""

from __future__ import annotations

import json
from pathlib import Path

from generate_registered_callback_family import (
    ROOT, SOURCE, call, control_flow, decode_source, full_address,
    hex32, normalized, require,
)


CANDIDATES = ROOT / "out/function-inventory/registered-dependency-candidates.json"
SHAPES = ROOT / "out/function-inventory/registered-dependency-shapes.json"
CONSTRUCTORS = ROOT / "LostOdysseyRecompSemantics/registered_constructor_families.json"
NEXT = ROOT / "out/function-inventory/registered-next-dependencies.json"
OUTPUT = ROOT / "LostOdysseyRecompSemantics/registered_dependency_registration_families.json"
START = "    // BEGIN GENERATED REGISTERED DEPENDENCY PARAMETERS"
END = "    // END GENERATED REGISTERED DEPENDENCY PARAMETERS"

# self, parent, meta, primary, primary ctor, graph, gate, ready, field stores.
LAYOUT = {
    1: (14, ("lazy", 4, 9, 11, 17), ("lazy", 26, 30, 32),
        37, 41, 43, 46, 53, (19, 22, 36, 49)),
    2: (15, ("lazy", 6, 10, 12, 18), ("lazy", 24, 28, 30),
        35, 39, 41, 44, 51, (20, 23, 34, 47)),
    3: (5, ("direct", 3, 8), ("lazy", 17, 21, 23),
        28, 32, 34, 37, 44, (10, 13, 27, 40)),
    4: (15, ("lazy", 6, 10, 12, 18), ("lazy", 25, 29, 31),
        36, 40, 42, 45, 52, (20, 23, 35, 48)),
    6: (15, ("lazy", 6, 10, 12, 18), ("lazy", 25, 30, 32),
        37, 41, 43, 46, 53, (20, 23, 36, 49)),
    7: (7, ("direct", 5, 10), ("lazy", 17, 22, 24),
        29, 34, 36, 39, 46, (12, 15, 28, 42)),
}


def field_store(instruction: str) -> int:
    require(instruction.startswith("stw ") and "(" in instruction,
            f"expected field store: {instruction}")
    return int(instruction.split(",", 1)[1].split("(", 1)[0])


def main() -> None:
    candidates = json.loads(CANDIDATES.read_text(encoding="utf-8"))
    shapes = json.loads(SHAPES.read_text(encoding="utf-8"))
    constructors = {int(entry["address"], 16): entry for entry in
                    json.loads(CONSTRUCTORS.read_text(encoding="utf-8"))["entries"]}
    next_entries = {int(item["address"], 16): item for item in
                    json.loads(NEXT.read_text(encoding="utf-8"))}
    require(0x827ce240 in next_entries, "inline constructor cache missing")
    constructors[0x827ce240] = {"kind": "constructor"}
    group_of = {int(address, 16): group for group, shape in enumerate(shapes)
                for address in shape["addresses"]}
    selected = [candidate for candidate in candidates
                if group_of[int(candidate["address"], 16)] in LAYOUT]
    require(len(selected) == 51, "registration dependency count changed")
    first = {group: next(candidate for candidate in selected
             if group_of[int(candidate["address"], 16)] == group)
             for group in LAYOUT}
    flow = {group: control_flow(int(candidate["address"], 16),
            candidate["instructions"]) for group, candidate in first.items()}
    known_registration = ({int(item["address"], 16) for item in selected}
                          | set(next_entries)
                          | {0x82403200})

    entries = []
    for candidate in selected:
        address = int(candidate["address"], 16)
        group = group_of[address]
        instructions = candidate["instructions"]
        require([normalized(value) for value in instructions] ==
                shapes[group]["instruction_shape"],
                f"instruction skeleton changed: {address:08X}")
        require(control_flow(address, instructions) == flow[group],
                f"branch destinations changed: {address:08X}")
        require("stwu r1,-112(r1)" in instructions,
                f"frame changed: {address:08X}")
        self_index, parent_layout, meta_layout, primary_index, \
            ctor_index, graph_index, gate_index, ready_index, stores = LAYOUT[group]
        require(tuple(field_store(instructions[index]) for index in stores) ==
                (60, 60, 196, 52), f"field layout changed: {address:08X}")
        require(sum(",124(" in value for value in instructions) == 1,
                f"ready vtable slot changed: {address:08X}")
        parent = decode_source(instructions, parent_layout, constructors, "parent")
        meta = decode_source(instructions, meta_layout, constructors, "meta")
        require(meta["mode"] == "lazy", f"meta mode changed: {address:08X}")
        for role, source in (("parent", parent), ("meta", meta)):
            if source["registration"] is not None:
                require(int(source["registration"], 16) in known_registration,
                        f"{role} registration left classified set: {address:08X}")
        primary = full_address(instructions, primary_index)
        gate = full_address(instructions, gate_index)
        require(primary == 0x83315f9c and gate == 0x83315ed8,
                f"primary/gate changed: {address:08X}")
        require(call(instructions, ctor_index) == 0x82410b90 and
                call(instructions, graph_index) == 0x82410c48 and
                call(instructions, ready_index) == 0x82406b00,
                f"primary call chain changed: {address:08X}")
        entries.append({"address": hex32(address),
                        "source": candidate["generated_ppc_path"],
                        "source_line": candidate["line"],
                        "shape_id": group, "frame_size": 112,
                        "self_global": hex32(full_address(instructions, self_index)),
                        "parent": parent, "meta": meta,
                        "primary_global": hex32(primary),
                        "ready_gate": hex32(gate),
                        "ready_gate_before_primary_store": True,
                        "ready_capture_from_self_field": True})
    entries.sort(key=lambda entry: entry["address"])
    require(len({entry["address"] for entry in entries}) == 51,
            "duplicate registration dependency")
    payload = {"schema_version": 1,
               "source": str(CANDIDATES.relative_to(ROOT)).replace("\\", "/"),
               "family": "registered_dependency_registration",
               "entry_count": 51, "entries": entries}
    output = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"
    if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != output:
        OUTPUT.write_text(output, encoding="utf-8")

    source = SOURCE.read_text(encoding="utf-8")
    require(source.count(START) == source.count(END) == 1,
            "generated source markers missing")
    rows = []
    for entry in entries:
        fields = [entry["address"], str(100 + entry["shape_id"]), "112",
                  entry["self_global"]]
        for item in (entry["parent"], entry["meta"]):
            fields.append("{" + ", ".join([
                "true" if item["mode"] == "lazy" else "false",
                item["getter"] or "0", item["global"] or "0",
                item["constructor"] or "0", item["registration"] or "0",
                item["descriptor"] or "0"])
                + "}")
        fields.append("false")
        rows.append("    {" + ", ".join(fields) + "},")
    start = source.index(START) + len(START)
    end = source.index(END)
    updated = source[:start] + "\n" + "\n".join(rows) + "\n    " + source[end:]
    if updated != source:
        SOURCE.write_text(updated, encoding="utf-8")
    print("classified 51 registered dependency callbacks")


if __name__ == "__main__":
    main()
