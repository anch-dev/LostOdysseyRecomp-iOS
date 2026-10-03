"""Name recovered registration entries from their known UTF-16BE pointers.

Reads only the referenced string ranges. This does not scan PPC bodies, hash the
image, infer function equivalence, or change recovery/validation counts.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
IMAGE_BASE = 0x82000000


def entries(name: str) -> list[dict]:
    return json.loads((SEMANTICS / name).read_text(encoding="utf-8"))["entries"]


def address(value: str) -> str:
    return f"{int(value, 16) & 0xFFFFFFFF:08X}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True,
                        help="Decompressed disc-1 image mapped at 0x82000000")
    args = parser.parse_args()
    constructors = entries("registered_constructor_families.json")
    # These two constructors have individual implementations rather than table
    # entries. Constants match their reviewed original PPC call arguments.
    constructors += [
        {"address": "82410B90", "kind": "individual_constructor",
         "descriptor": "0xFFFFFFFF821913EE", "object_size": 376,
         "category": 0x10000000},
        {"address": "827CE240", "kind": "individual_constructor",
         "descriptor": "0xFFFFFFFF82021F52", "object_size": 76,
         "category": 1},
    ]
    getters: dict[str, list[dict]] = {}
    for entry in entries("registered_getter_families.json"):
        getters.setdefault(address(entry["constructor"]), []).append(entry)
    # The primary getter and its registrar were recovered individually.
    getters["82410B90"] = [{"address": "82406B00", "registration": "82410C48"}]

    names: dict[int, str] = {}
    image_size = args.image.stat().st_size
    with args.image.open("rb") as image:
        def read_name(pointer: str) -> str:
            guest = int(pointer, 16) & 0xFFFFFFFF
            if guest in names:
                return names[guest]
            offset = guest - IMAGE_BASE
            if offset < 0 or offset + 2 > image_size or offset % 2:
                raise ValueError(f"Invalid descriptor pointer: {guest:08X}")
            image.seek(offset)
            raw = image.read(min(512, image_size - offset))
            end = next((i for i in range(0, len(raw) - 1, 2)
                        if raw[i:i + 2] == b"\0\0"), None)
            if end is None:
                raise ValueError(f"Unterminated descriptor: {guest:08X}")
            name = raw[:end].decode("utf-16-be")
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
                raise ValueError(f"Unexpected descriptor at {guest:08X}: {name!r}")
            names[guest] = name
            return name

        rows = []
        for entry in constructors:
            constructor = address(entry["address"])
            linked = getters.get(constructor, [])
            lazy = entry["kind"] == "lazy_singleton"
            callbacks = {address(g["registration"]) for g in linked}
            if entry.get("post_callback"):
                callbacks.add(address(entry["post_callback"]))
            rows.append({
                "type_name": read_name(entry["descriptor"]),
                "descriptor_address": address(entry["descriptor"]),
                "entry_address": constructor,
                "entry_kind": entry["kind"],
                "declared_size": entry["object_size"],
                "category_bits": f"0x{entry['category']:08X}",
                "getter_addresses": sorted({address(g["address"]) for g in linked}
                                           | ({constructor} if lazy else set())),
                "registration_addresses": sorted(callbacks),
            })
    rows.sort(key=lambda row: (row["type_name"].casefold(), row["entry_address"]))
    catalog = {
        "schema_version": 1,
        "source_image": args.image.name,
        "image_base": "0x82000000",
        "encoding": "UTF-16BE, null terminated",
        "entry_count": len(rows),
        "scope": "Literal names at the descriptor pointers of recovered constructors. "
                 "Getter/registration links come from existing recovery manifests. "
                 "Declared size is the initializer argument, not the 376-byte metadata allocation. "
                 "This catalog does not establish original C++ layouts or add recovered functions.",
        "entries": rows,
    }
    (SEMANTICS / "registered_type_catalog.json").write_text(
        json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# Registered type names", "",
        "Literal UTF-16BE names read at the recovered descriptor pointers in disc 1.",
        "Entry addresses identify recovered constructors or lazy singleton functions.",
        "Declared size is the initializer argument; the metadata allocation itself is 376 bytes.",
        "This index adds names and navigation, not recovered functions or layout claims.", "",
        "Machine-readable pointers and category bits: [registered_type_catalog.json](registered_type_catalog.json).", "",
        "| Type name | Constructor / lazy entry | Getter | Registration | Declared size |",
        "| --- | --- | --- | --- | ---: |",
    ]
    for row in rows:
        lines.append(f"| {row['type_name']} | `{row['entry_address']}` | "
                     f"{', '.join(row['getter_addresses']) or '-'} | "
                     f"{', '.join(row['registration_addresses']) or '-'} | {row['declared_size']} |")
    (SEMANTICS / "REGISTERED_TYPES.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Named {len(rows)} recovered constructor/lazy entries from {len(names)} exact string pointers.")


if __name__ == "__main__":
    main()
