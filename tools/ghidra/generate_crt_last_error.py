"""Admit the exact last-error getter and its real tail entry."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/crt_last_error_families.json"
SPECS = (('822CA108', 'LostOdysseyRecompLib/ppc/ppc_recomp.2.cpp', 40799, 8, 'PPC_FUNC_IMPL(__imp__sub_822CA108) {\n\tPPC_FUNC_PROLOGUE();\n\t// lwz r11,336(r13)\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r13.u32 + 336);\n\t// cmplwi cr6,r11,0\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n\t// bne cr6,0x822ca120\n\tif (!ctx.cr6.eq) goto loc_822CA120;\n\t// lwz r11,256(r13)\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r13.u32 + 256);\n\t// lwz r3,352(r11)\n\tctx.r3.u64 = PPC_LOAD_U32(ctx.r11.u32 + 352);\n\t// blr \n\treturn;\nloc_822CA120:\n\t// li r3,0\n\tctx.r3.s64 = 0;\n\t// blr \n\treturn;\n}'), ('822CA100', 'LostOdysseyRecompLib/ppc/ppc_recomp.2.cpp', 40788, 1, 'PPC_FUNC_IMPL(__imp__sub_822CA100) {\n\tPPC_FUNC_PROLOGUE();\n\t// b 0x822ca108\n\tsub_822CA108(ctx, base);\n\treturn;\n}'))


def generate() -> dict:
    entries = []
    sources = {}
    for address, path, line, count, expected in SPECS:
        source = ROOT / path
        if not source.exists():
            source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / path
        if path not in sources:
            sources[path] = source.read_text(encoding="utf-8").splitlines()
        lines = sources[path]
        if lines[line - 1] != f"PPC_FUNC_IMPL(__imp__sub_{address}) {{":
            raise ValueError(f"{address} source location changed")
        end = next(i for i in range(line, len(lines)) if lines[i] == "}")
        observed = "\n".join(lines[line - 1:end + 1])
        if observed != expected:
            raise ValueError(f"{address} complete translated body changed")
        instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
        if len(instructions) != count:
            raise ValueError(f"{address} instruction count changed")
        entries.append({"address": address, "source": path,
            "source_line": line, "instructions": instructions,
            "direct_calls": re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", observed),
            "cfg": {"labels": re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE),
                "branches": re.findall(r"^[ \t]*// (b[^\n]*)$", observed, re.MULTILINE)},
            "translated_body": observed, "status": "bounded_readable_library",
            "boundary": "Complete selected getter/tail PPCContext and unchanged ordinary RAM; faults/MMIO/concurrency and runtime not verified."})
    return {"schema": "crt-last-error-v1", "entries": entries}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(), indent=2) + "\n"
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")

if __name__ == "__main__":
    main()
