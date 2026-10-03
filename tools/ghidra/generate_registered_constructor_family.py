"""Project the reviewed constructor parameters into a readable C++ table."""

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED REGISTERED CONSTRUCTOR PARAMETERS\n"
END = "    // END GENERATED REGISTERED CONSTRUCTOR PARAMETERS\n"
KINDS = {
    "constructor": "Constructor",
    "constructor_stack_scratch": "ConstructorWithScratch",
    "lazy_singleton": "LazySingleton",
}
ORDERS = {
    (92, 80, 108, 100): "Word92First",
    (100, 92, 108, 80): "Word100First",
}


def parse_hex(value, bits):
    if not isinstance(value, str) or not value.startswith("0x"):
        raise ValueError(f"expected hexadecimal value: {value!r}")
    result = int(value, 16)
    if result < 0 or result >= (1 << bits):
        raise ValueError(f"out-of-range {bits}-bit value: {value}")
    return result


def normalize(entry):
    address = int(entry["address"], 16)
    kind = entry["variant"]
    if kind not in KINDS or entry["allocation_size"] != 376:
        raise ValueError(f"unexpected constructor variant at {address:08X}")
    if entry["preserve_full_allocation_and_initializer_r3"] is not True:
        raise ValueError(f"full r3 contract missing at {address:08X}")
    singleton = kind == "lazy_singleton"
    if singleton != (entry["return_kind"] == "zero_extended_reloaded_singleton_word"):
        raise ValueError(f"return contract changed at {address:08X}")
    if singleton != (entry["singleton_address"] is not None and
                     entry["post_callback"] is not None):
        raise ValueError(f"singleton fields changed at {address:08X}")
    if entry["frame_size"] != (144 if kind == "constructor_stack_scratch" else 128):
        raise ValueError(f"frame size changed at {address:08X}")
    if entry["allocation_scratch_offset"] != (112 if kind == "constructor_stack_scratch" else None):
        raise ValueError(f"scratch offset changed at {address:08X}")

    stores = entry["ordered_outgoing_stores"]
    order = tuple(store["offset"] for store in stores)
    if order not in ORDERS or [store["width"] for store in stores] != [
        64 if offset == 80 else 32 for offset in order]:
        raise ValueError(f"outgoing stores changed at {address:08X}")
    words = {store["offset"]: parse_hex(store["value"], store["width"])
             for store in stores}
    if words[80] != 0x0408408400004000:
        raise ValueError(f"flags changed at {address:08X}")

    parameters = entry["parameters"]
    if parse_hex(parameters["byte_register"], 64) != 0:
        raise ValueError(f"byte register changed at {address:08X}")
    owner_is_incoming = parameters["owner_register"] == "incoming_r3"
    owner = 0 if owner_is_incoming else parse_hex(parameters["owner_register"], 64)
    normalized = {
        "address": f"{address:08X}",
        "source": entry["source"],
        "source_line": entry["source_line"],
        "kind": kind,
        "frame_size": entry["frame_size"],
        "scratch_offset": entry["allocation_scratch_offset"],
        "singleton_address": entry["singleton_address"],
        "post_callback": entry["post_callback"],
        "store_order": list(order),
        "object_size": parse_hex(parameters["size_register"], 32),
        "category": parse_hex(parameters["category_register"], 32),
        "descriptor": f"0x{parse_hex(parameters['descriptor_register'], 64):016X}",
        "owner": "incoming_r3" if owner_is_incoming else f"0x{owner:016X}",
        "tag": f"0x{parse_hex(parameters['tag_register'], 64):016X}",
        "outgoing92": f"0x{words[92]:08X}",
        "outgoing100": f"0x{words[100]:08X}",
        "outgoing108": f"0x{words[108]:08X}",
    }
    if singleton:
        parse_hex(normalized["singleton_address"], 32)
        int(normalized["post_callback"], 16)
    return normalized


def cpp_row(entry):
    owner_is_incoming = entry["owner"] == "incoming_r3"
    owner = "0x0000000000000000ull" if owner_is_incoming else entry["owner"].lower() + "ull"
    singleton = entry["singleton_address"] or "0x00000000"
    callback = "0x" + (entry["post_callback"] or "00000000")
    values = [
        "0x" + entry["address"].lower() + "u",
        "Kind::" + KINDS[entry["kind"]],
        "StoreOrder::" + ORDERS[tuple(entry["store_order"])],
        singleton.lower() + "u", callback.lower() + "u",
        f"{entry['object_size']}u", f"0x{entry['category']:08x}u",
        entry["descriptor"].lower() + "ull", owner,
        entry["tag"].lower() + "ull",
        entry["outgoing92"].lower() + "u",
        entry["outgoing100"].lower() + "u",
        entry["outgoing108"].lower() + "u",
        "true" if owner_is_incoming else "false",
    ]
    return "    {" + ", ".join(values) + "},\n"


def write_if_changed(path, content):
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT /
                        "out/function-inventory/registered-constructor-families.json")
    parser.add_argument("--manifest", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/registered_constructor_families.json")
    parser.add_argument("--source", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp")
    args = parser.parse_args()
    data = json.loads(args.input.read_text(encoding="utf-8"))
    entries = [normalize(entry) for entry in data["entries"]]
    addresses = [int(entry["address"], 16) for entry in entries]
    if len(entries) != 846 or len(set(addresses)) != len(entries):
        raise ValueError("reviewed family count or uniqueness changed")
    if "82410B90" not in {entry["address"] for entry in entries}:
        raise ValueError("expected individually recovered constructor missing")
    entries = [entry for entry in entries if entry["address"] != "82410B90"]
    entries.sort(key=lambda entry: int(entry["address"], 16))
    manifest = {
        "schema_version": 1,
        "source": "out/function-inventory/registered-constructor-families.json",
        "family": "registered_constructor",
        "excluded_individual_overlap": ["82410B90"],
        "entry_count": len(entries),
        "entries": entries,
    }
    write_if_changed(args.manifest, json.dumps(manifest, indent=2) + "\n")

    source = args.source.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("C++ parameter table markers are missing or duplicated")
    prefix, remainder = source.split(BEGIN, 1)
    _, suffix = remainder.split(END, 1)
    rows = "".join(cpp_row(entry) for entry in entries)
    write_if_changed(args.source, prefix + BEGIN + rows + END + suffix)
    print(f"generated {len(entries)} registered constructor parameters")


if __name__ == "__main__":
    main()
