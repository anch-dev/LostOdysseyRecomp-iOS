"""Pin the complete generated PPC body of the 827CD7BC heap-lock exit."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/heap_lock_exit_families.json"
ADDRESS = "827CD7BC"
SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp"
LINE = 5592
EXPECTED = "PPC_FUNC_IMPL(__imp__sub_827CD7BC) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// std r31,-8(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -8, ctx.r31.u64);\n\t// addi r31,r12,-320\n\tctx.r31.s64 = ctx.r12.s64 + -320;\n\t// std r22,-16(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -16, ctx.r22.u64);\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-24(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -24, ctx.r12.u32);\n\t// stwu r1,-112(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-112);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// lwz r11,96(r31)\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 96);\n\t// cmplwi cr6,r11,0\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n\t// beq cr6,0x827cd7e8\n\tif (ctx.cr6.eq) goto loc_827CD7E8;\n\t// lwz r3,1408(r22)\n\tctx.r3.u64 = PPC_LOAD_U32(ctx.r22.u32 + 1408);\n\t// bl 0x830d9c7c\n\tctx.lr = 0x827CD7E8;\n\t__imp__RtlLeaveCriticalSection(ctx, base);\nloc_827CD7E8:\n\t// lwz r1,0(r1)\n\tctx.r1.u64 = PPC_LOAD_U32(ctx.r1.u32 + 0);\n\t// ld r31,-8(r1)\n\tctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);\n\t// ld r22,-16(r1)\n\tctx.r22.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);\n\t// lwz r12,-24(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -24);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr \n\treturn;\n}"


def generate() -> dict:
    source = ROOT / SOURCE
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    if lines[LINE - 1] != f"PPC_FUNC_IMPL(__imp__sub_{ADDRESS}) {{":
        raise ValueError("heap-lock-exit fixed source location changed")
    end = next(i for i in range(LINE, len(lines)) if lines[i] == "}")
    observed = "\n".join(lines[LINE - 1:end + 1])
    if observed != EXPECTED:
        raise ValueError("heap-lock-exit complete translated body changed")
    instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
    if len(instructions) != 17:
        raise ValueError("heap-lock-exit instruction count changed")
    if re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", observed):
        raise ValueError("heap-lock-exit gained a PPC direct call")
    imports = re.findall(r"\b__imp__([A-Za-z0-9_]+)\(ctx, base\);", observed)
    if imports != ["RtlLeaveCriticalSection"]:
        raise ValueError("heap-lock-exit native call changed")
    entry = {"address": ADDRESS, "source": SOURCE, "source_line": LINE,
        "instructions": instructions, "direct_calls": [],
        "native_calls": imports,
        "cfg": {
            "labels": re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE),
            "branches": re.findall(r"^[ \t]*// (b[^\n]*)$", observed, re.MULTILINE),
        },
        "translated_body": observed,
        "status": "bounded_readable_library",
        "boundary": "Complete selected registers, own frame and ordinary RAM; RtlLeaveCriticalSection explicit native service. Other volatile registers, faults, MMIO, concurrency and runtime unverified."}
    return {"schema": "heap-lock-exit-v1", "entries": [entry]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(), indent=2) + "\n"
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")


if __name__ == "__main__":
    main()
