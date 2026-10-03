"""Generate the three reviewed no-call metadata initializer entries from cache.

Every complete PPC instruction list is matched; this never scans generated PPC.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = "out/function-inventory/registered-extra-methods.json"
MANIFEST = ROOT / "LostOdysseyRecompSemantics/registered_metadata_initializer_families.json"

SPECS = {
    "825A8448": {
        "instructions": [
            "lis r11,-32256", "lwz r10,60(r3)", "oris r10,r10,32768",
            "lfs f0,3664(r11)", "lis r11,-32231", "stfs f0,76(r3)",
            "stfs f0,72(r3)", "stw r10,60(r3)", "stfs f0,68(r3)",
            "stfs f0,64(r3)", "lfs f13,-27252(r11)", "stfs f13,92(r3)",
            "stfs f13,88(r3)", "stfs f13,84(r3)", "stfs f13,80(r3)", "blr",
        ],
        "reads": ["0x82000E50", "0x8218958C", "object+60"],
        "writes": ["object+76", "object+72", "object+60", "object+68",
                   "object+64", "object+92", "object+88", "object+84", "object+80"],
    },
    "826DA6B0": {
        "instructions": [
            "lis r10,-32231", "li r11,1", "lfs f0,-27252(r10)",
            "lis r10,-32256", "stfs f0,84(r3)", "stw r11,208(r3)",
            "stw r11,212(r3)", "stw r11,220(r3)", "lfs f13,3204(r10)",
            "li r10,0", "stfs f13,88(r3)", "stw r10,216(r3)", "blr",
        ],
        "reads": ["0x8218958C", "0x82000C84"],
        "writes": ["object+84", "object+208", "object+212", "object+220",
                   "object+88", "object+216"],
    },
    "8270BBE8": {
        "instructions": [
            "lis r11,-32231", "li r10,1", "addi r11,r11,-27648",
            "stw r10,80(r3)", "lfs f0,248(r11)", "lfs f13,0(r11)",
            "lis r11,-31965", "stfs f0,72(r3)", "stfs f13,76(r3)",
            "lwz r11,23268(r11)", "stw r11,84(r3)", "lis r11,-31965",
            "lwz r11,23272(r11)", "stw r11,88(r3)", "blr",
        ],
        "reads": ["0x821894F8", "0x82189400", "0x83235AE4", "0x83235AE8"],
        "writes": ["object+80", "object+72", "object+76", "object+84", "object+88"],
    },
}


def generate(candidates: list[dict]) -> dict:
    by_address = {item["address"]: item for item in candidates}
    if len(by_address) != len(candidates):
        raise ValueError("duplicate cached metadata entry")
    old = json.loads((ROOT / "LostOdysseyRecompSemantics/registered_metadata_word_families.json")
                     .read_text(encoding="utf-8"))
    old_addresses = {item["address"] for item in old["entries"]}
    if set(SPECS) & old_addresses:
        raise ValueError("initializer overlaps an existing metadata mapping")
    entries = []
    for address, spec in sorted(SPECS.items()):
        candidate = by_address.get(address)
        if candidate is None or [line.strip() for line in candidate["instructions"]] != spec["instructions"]:
            raise ValueError(f"unreviewed metadata initializer body: {address}")
        if not candidate["body"].startswith(f"PPC_FUNC_IMPL(__imp__sub_{address})"):
            raise ValueError(f"unexpected cached PPC body: {address}")
        entries.append({
            "address": address,
            "source": candidate["generated_ppc_path"],
            "source_line": candidate["line"],
            "reads": spec["reads"],
            "writes_in_order": spec["writes"],
            "status": "bounded_memory_register_semantics",
        })
    return {"schema_version": 1, "family": "registered_metadata_initializer",
            "source": SOURCE, "entry_count": len(entries), "complete": False,
            "format_model": "PPC Programming Environments Rev. 1 Appendix D.6/D.7; LoadedSingle uses integer format movement and preserves signaling NaN bits, without arbitrary-double stfs or FP arithmetic",
            "format_reference": "https://www.nxp.com/docs/en/user-guide/MPCFPE.pdf",
            "fp_service": "DisableFlushMode callback at the first lfs, after preceding integer effects; unknown entry makes no callback",
            "limitations": ["known_signaling_nan_generated_cpp_fpr_differential",
                            "no_runtime_wrapper_or_scene"],
            "known_differential": {
                "address": "825A8448",
                "source_word": "0x7F800001",
                "generated_cpp_o2_f0_bits": "0x7FF8000020000000",
                "documented_ppc_f0_bits": "0x7FF0000020000000",
                "generated_cpp_o2_store": "0x7F800001",
                "documented_ppc_store": "0x7F800001",
                "hardware_measured": False,
            },
            "historical_host_cast_differential": {
                "address": "825A8448",
                "source_address": "0x82000E50",
                "source_word": "0x7F800001",
                "object_address": "0x00010000",
                "object_store_address": "0x00010040",
                "generated_cpp_o2_store": "0x7F800001",
                "semantic_o2_store": "0x7FC00001",
                "generated_cpp_od_store": "0x7FC00001",
                "f0_double_bits_both": "0x7FF8000020000000",
                "fpscr_csr_both": "0x00001F80",
            },
            "entries": entries}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    candidates = json.loads((ROOT / SOURCE).read_text(encoding="utf-8"))
    text = json.dumps(generate(candidates), indent=2) + "\n"
    if args.check:
        if not MANIFEST.exists() or MANIFEST.read_text(encoding="utf-8") != text:
            raise ValueError("registered metadata initializer manifest differs")
    else:
        MANIFEST.write_text(text, encoding="utf-8", newline="\n")
    print("Validated three complete cached metadata initializer instruction lists.")


if __name__ == "__main__":
    main()
