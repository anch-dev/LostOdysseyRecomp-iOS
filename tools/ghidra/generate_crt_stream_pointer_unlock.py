"""Pin four complete stream-pointer unlock generated PPC bodies."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/crt_stream_pointer_unlock_families.json"
SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.176.cpp"
SPECS = (
    ("82B863F0", 16030, 9, "PPC_FUNC_IMPL(__imp__sub_82B863F0) {\n\tPPC_FUNC_PROLOGUE();\n\t// srawi r10,r3,5\n\tctx.xer.ca = (ctx.r3.s32 < 0) & ((ctx.r3.u32 & 0x1F) != 0);\n\tctx.r10.s64 = ctx.r3.s32 >> 5;\n\t// lis r11,-31944\n\tctx.r11.s64 = -2093481984;\n\t// rlwinm r9,r10,2,0,29\n\tctx.r9.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0xFFFFFFFC;\n\t// addi r11,r11,-29312\n\tctx.r11.s64 = ctx.r11.s64 + -29312;\n\t// rlwinm r10,r3,6,21,25\n\tctx.r10.u64 = __builtin_rotateleft64(ctx.r3.u32 | (ctx.r3.u64 << 32), 6) & 0x7C0;\n\t// lwzx r11,r9,r11\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r11.u32);\n\t// add r11,r11,r10\n\tctx.r11.u64 = ctx.r11.u64 + ctx.r10.u64;\n\t// addi r3,r11,12\n\tctx.r3.s64 = ctx.r11.s64 + 12;\n\t// b 0x830d9c7c\n\t__imp__RtlLeaveCriticalSection(ctx, base);\n\treturn;\n}", [], ["RtlLeaveCriticalSection"], "r3 full, SP/LR and live selected volatile registers"),
    ("82B81F38", 5157, 14, "PPC_FUNC_IMPL(__imp__sub_82B81F38) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// std r31,-8(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -8, ctx.r31.u64);\n\t// addi r31,r12,-160\n\tctx.r31.s64 = ctx.r12.s64 + -160;\n\t// std r30,-16(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -16, ctx.r30.u64);\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-24(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -24, ctx.r12.u32);\n\t// stwu r1,-112(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-112);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// mr r3,r30\n\tctx.r3.u64 = ctx.r30.u64;\n\t// bl 0x82b863f0\n\tctx.lr = 0x82B81F58;\n\tsub_82B863F0(ctx, base);\n\t// lwz r1,0(r1)\n\tctx.r1.u64 = PPC_LOAD_U32(ctx.r1.u32 + 0);\n\t// ld r31,-8(r1)\n\tctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);\n\t// ld r30,-16(r1)\n\tctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);\n\t// lwz r12,-24(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -24);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr \n\treturn;\n}", ["82B863F0"], [], "r12/r30/r31 full, SP/LR and live saved guest slots"),
    ("82B860CC", 15538, 14, "PPC_FUNC_IMPL(__imp__sub_82B860CC) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// std r31,-8(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -8, ctx.r31.u64);\n\t// addi r31,r12,-160\n\tctx.r31.s64 = ctx.r12.s64 + -160;\n\t// std r30,-16(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -16, ctx.r30.u64);\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-24(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -24, ctx.r12.u32);\n\t// stwu r1,-112(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-112);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// mr r3,r30\n\tctx.r3.u64 = ctx.r30.u64;\n\t// bl 0x82b863f0\n\tctx.lr = 0x82B860EC;\n\tsub_82B863F0(ctx, base);\n\t// lwz r1,0(r1)\n\tctx.r1.u64 = PPC_LOAD_U32(ctx.r1.u32 + 0);\n\t// ld r31,-8(r1)\n\tctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);\n\t// ld r30,-16(r1)\n\tctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);\n\t// lwz r12,-24(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -24);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr \n\treturn;\n}", ["82B863F0"], [], "r12/r30/r31 full, SP/LR and live saved guest slots"),
    ("82B81AF8", 4466, 11, "PPC_FUNC_IMPL(__imp__sub_82B81AF8) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// std r30,-8(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -8, ctx.r30.u64);\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-16(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -16, ctx.r12.u32);\n\t// stwu r1,-96(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-96);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// lwz r3,80(r30)\n\tctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + 80);\n\t// bl 0x830d9c7c\n\tctx.lr = 0x82B81B10;\n\t__imp__RtlLeaveCriticalSection(ctx, base);\n\t// lwz r1,0(r1)\n\tctx.r1.u64 = PPC_LOAD_U32(ctx.r1.u32 + 0);\n\t// ld r30,-8(r1)\n\tctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);\n\t// lwz r12,-16(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -16);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr \n\treturn;\n}", [], ["RtlLeaveCriticalSection"], "r30 full, SP/LR and live saved guest slots"),
)


def generate() -> dict:
    source = ROOT / SOURCE
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    entries = []
    for address, line, count, expected, direct_expected, native_expected, inputs in SPECS:
        if lines[line - 1] != f"PPC_FUNC_IMPL(__imp__sub_{address}) {{":
            raise ValueError(f"{address} fixed source location changed")
        end = next(i for i in range(line, len(lines)) if lines[i] == "}")
        observed = "\n".join(lines[line - 1:end + 1])
        if observed != expected:
            raise ValueError(f"{address} complete translated body changed")
        instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
        direct = re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", observed)
        native = re.findall(r"\b__imp__([A-Za-z0-9_]+)\(ctx, base\);", observed)
        if len(instructions) != count or direct != direct_expected or native != native_expected:
            raise ValueError(f"{address} instruction or call sequence changed")
        if "PPC_CALL_INDIRECT_FUNC" in observed:
            raise ValueError(f"{address} gained an indirect PPC call")
        entries.append({"address": address, "source": SOURCE, "source_line": line,
            "instructions": instructions, "direct_calls": direct,
            "native_calls": native, "inputs": inputs,
            "cfg": {"labels": re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE),
                "branches": re.findall(r"^[ \t]*// (b[^\n]*)$", observed, re.MULTILINE)},
            "translated_body": observed, "status": "bounded_readable_library",
            "boundary": "Complete selected registers and ordinary RAM; RtlLeaveCriticalSection native tail service explicit; unselected native volatile effects, faults, MMIO, concurrency and runtime unverified."})
    return {"schema": "crt-stream-pointer-unlock-v1", "entries": entries}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(), indent=2) + "\n"
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")


if __name__ == "__main__":
    main()
