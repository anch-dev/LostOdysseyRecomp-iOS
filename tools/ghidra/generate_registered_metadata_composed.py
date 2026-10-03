"""Admit the exact virtual thunk and UTF-16 copy helper used by metadata."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import write_if_changed

ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "registered_metadata_composed_families.json"
CACHE = ROOT / "out/function-inventory/registered-extra-methods.json"
COPY_CACHE = ROOT / "out/function-inventory/registered-metadata-composed-helper.json"
COPY_LINE = 46083

# Every translated statement and control-flow label is fixed here. Whitespace
# is ignored, but changed instructions, targets, or translated operations fail.
EXPECTED_LINES = {
    "826D6C10": [
        "PPC_FUNC_IMPL(__imp__sub_826D6C10) {",
        "PPC_FUNC_PROLOGUE();",
        "// lwz r12,0(r3)",
        "ctx.r12.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);",
        "// lwz r11,292(r12)",
        "ctx.r11.u64 = PPC_LOAD_U32(ctx.r12.u32 + 292);",
        "// mtctr r11",
        "ctx.ctr.u64 = ctx.r11.u64;",
        "// bctr",
        "PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);",
        "return;",
        "}",
    ],
    "8230BAC0": [
        "PPC_FUNC_IMPL(__imp__sub_8230BAC0) {",
        "PPC_FUNC_PROLOGUE();",
        "// mr r11,r3",
        "ctx.r11.u64 = ctx.r3.u64;",
        "loc_8230BAC4:",
        "// lhz r10,0(r4)",
        "ctx.r10.u64 = PPC_LOAD_U16(ctx.r4.u32 + 0);",
        "// addi r4,r4,2",
        "ctx.r4.s64 = ctx.r4.s64 + 2;",
        "// cmplwi r10,0",
        "ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);",
        "// sth r10,0(r11)",
        "PPC_STORE_U16(ctx.r11.u32 + 0, ctx.r10.u16);",
        "// addi r11,r11,2",
        "ctx.r11.s64 = ctx.r11.s64 + 2;",
        "// bne 0x8230bac4",
        "if (!ctx.cr0.eq) goto loc_8230BAC4;",
        "// blr",
        "return;",
        "}",
    ],
}


def lines(body: str) -> list[str]:
    return [line.strip() for line in body.splitlines() if line.strip()]


def copy_body() -> str:
    cached = json.loads(COPY_CACHE.read_text(encoding="utf-8"))
    if cached["address"] != "8230BAC0" or cached["source_line"] != COPY_LINE or \
            cached["source"] != "LostOdysseyRecompLib/ppc/ppc_recomp.4.cpp":
        raise ValueError("8230BAC0 cached source identity changed")
    return cached["body"]


def generate() -> dict:
    cache = json.loads(CACHE.read_text(encoding="utf-8"))
    matching = [entry for entry in cache if entry["address"] == "826D6C10"]
    if len(matching) != 1:
        raise ValueError("826D6C10 missing or duplicate cached body")
    thunk = matching[0]
    admitted = [
        ("8230BAC0", copy_body(), "LostOdysseyRecompLib/ppc/ppc_recomp.4.cpp",
         COPY_LINE, "utf16_copy_until_null"),
        ("826D6C10", thunk["body"], thunk["generated_ppc_path"],
         thunk["line"], "virtual_tail_call"),
    ]
    entries = []
    for address, body, source, line, kind in admitted:
        if lines(body) != EXPECTED_LINES[address]:
            raise ValueError(f"{address}: complete translated body/CFG changed")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        if kind == "virtual_tail_call":
            if instructions != ["lwz r12,0(r3)", "lwz r11,292(r12)",
                                "mtctr r11", "bctr"]:
                raise ValueError("virtual method offset or dispatch changed")
        elif instructions != ["mr r11,r3", "lhz r10,0(r4)",
                             "addi r4,r4,2", "cmplwi r10,0",
                             "sth r10,0(r11)", "addi r11,r11,2",
                             "bne 0x8230bac4", "blr"]:
            raise ValueError("UTF-16 copy order or loop changed")
        entries.append({"address": address, "kind": kind,
                        "source": source, "source_line": line,
                        "instruction_sequence": instructions,
                        "translated_body": body,
                        "cfg": [part for part in instructions if
                                part.startswith(("bne ", "bctr", "blr"))],
                        "direct_calls": []})
    return {"schema_version": 1, "entry_count": 2,
            "metadata_caller_count": 1, "support_helper_count": 1,
            "entries": entries,
            "bounded_effects": "ordinary guest memory, ordered halfword copy, "
            "full r3/r4 and SP/LR callback values; vtable target is an "
            "explicit external tail-call boundary; volatile GPR/CR/CTR "
            "and target implementation are outside this model"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    encoded = json.dumps(generate(), indent=2) + "\n"
    if args.write:
        write_if_changed(MANIFEST, encoded)
    elif MANIFEST.read_text(encoding="utf-8") != encoded:
        raise ValueError("registered metadata composed manifest changed")
    print("PASS metadata composed 2 complete translated bodies/CFG")


if __name__ == "__main__":
    main()
