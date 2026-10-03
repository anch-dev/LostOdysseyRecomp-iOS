"""Pin three complete original CRT stream error and handle PPC bodies."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/crt_stream_error_families.json"
PINS = {
    "82B7FDB0": ("LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp", 21214, 14, ["822CA048"], "PPC_FUNC_IMPL(__imp__sub_82B7FDB0) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-8(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);\n\t// stwu r1,-96(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-96);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// bl 0x822ca048\n\tctx.lr = 0x82B7FDC0;\n\tsub_822CA048(ctx, base);\n\t// cmplwi r3,0\n\tctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n\t// bne 0x82b7fdd4\n\tif (!ctx.cr0.eq) goto loc_82B7FDD4;\n\t// lis r11,-31967\n\tctx.r11.s64 = -2094989312;\n\t// addi r3,r11,21012\n\tctx.r3.s64 = ctx.r11.s64 + 21012;\n\t// b 0x82b7fdd8\n\tgoto loc_82B7FDD8;\nloc_82B7FDD4:\n\t// addi r3,r3,12\n\tctx.r3.s64 = ctx.r3.s64 + 12;\nloc_82B7FDD8:\n\t// addi r1,r1,96\n\tctx.r1.s64 = ctx.r1.s64 + 96;\n\t// lwz r12,-8(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr \n\treturn;\n}"),
    "82B86228": ("LostOdysseyRecompLib/ppc/ppc_recomp.176.cpp", 15751, 50, ["82B7FDB0","82B7FD78","82B7FDB0","82B7FD78","82B7FEC0"], "PPC_FUNC_IMPL(__imp__sub_82B86228) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-8(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);\n\t// stwu r1,-96(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-96);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// cmpwi cr6,r3,-2\n\tctx.cr6.compare<int32_t>(ctx.r3.s32, -2, ctx.xer);\n\t// bne cr6,0x82b86260\n\tif (!ctx.cr6.eq) goto loc_82B86260;\n\t// bl 0x82b7fdb0\n\tctx.lr = 0x82B86240;\n\tsub_82B7FDB0(ctx, base);\n\t// li r11,0\n\tctx.r11.s64 = 0;\n\t// stw r11,0(r3)\n\tPPC_STORE_U32(ctx.r3.u32 + 0, ctx.r11.u32);\n\t// bl 0x82b7fd78\n\tctx.lr = 0x82B8624C;\n\tsub_82B7FD78(ctx, base);\n\t// mr r11,r3\n\tctx.r11.u64 = ctx.r3.u64;\n\t// li r10,9\n\tctx.r10.s64 = 9;\n\t// li r3,-1\n\tctx.r3.s64 = -1;\n\t// stw r10,0(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 0, ctx.r10.u32);\n\t// b 0x82b862e0\n\tgoto loc_82B862E0;\nloc_82B86260:\n\t// cmpwi cr6,r3,0\n\tctx.cr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n\t// blt cr6,0x82b86278\n\tif (ctx.cr6.lt) goto loc_82B86278;\n\t// lis r11,-31944\n\tctx.r11.s64 = -2093481984;\n\t// lwz r11,-29336(r11)\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + -29336);\n\t// cmplw cr6,r3,r11\n\tctx.cr6.compare<uint32_t>(ctx.r3.u32, ctx.r11.u32, ctx.xer);\n\t// blt cr6,0x82b862b4\n\tif (ctx.cr6.lt) goto loc_82B862B4;\nloc_82B86278:\n\t// bl 0x82b7fdb0\n\tctx.lr = 0x82B8627C;\n\tsub_82B7FDB0(ctx, base);\n\t// li r11,0\n\tctx.r11.s64 = 0;\n\t// stw r11,0(r3)\n\tPPC_STORE_U32(ctx.r3.u32 + 0, ctx.r11.u32);\n\t// bl 0x82b7fd78\n\tctx.lr = 0x82B86288;\n\tsub_82B7FD78(ctx, base);\n\t// mr r11,r3\n\tctx.r11.u64 = ctx.r3.u64;\n\t// li r10,9\n\tctx.r10.s64 = 9;\n\t// li r7,0\n\tctx.r7.s64 = 0;\n\t// li r6,0\n\tctx.r6.s64 = 0;\n\t// li r5,0\n\tctx.r5.s64 = 0;\n\t// li r4,0\n\tctx.r4.s64 = 0;\n\t// li r3,0\n\tctx.r3.s64 = 0;\n\t// stw r10,0(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 0, ctx.r10.u32);\n\t// bl 0x82b7fec0\n\tctx.lr = 0x82B862AC;\n\tsub_82B7FEC0(ctx, base);\n\t// li r3,-1\n\tctx.r3.s64 = -1;\n\t// b 0x82b862e0\n\tgoto loc_82B862E0;\nloc_82B862B4:\n\t// srawi r10,r3,5\n\tctx.xer.ca = (ctx.r3.s32 < 0) & ((ctx.r3.u32 & 0x1F) != 0);\n\tctx.r10.s64 = ctx.r3.s32 >> 5;\n\t// lis r11,-31944\n\tctx.r11.s64 = -2093481984;\n\t// rlwinm r9,r10,2,0,29\n\tctx.r9.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0xFFFFFFFC;\n\t// addi r11,r11,-29312\n\tctx.r11.s64 = ctx.r11.s64 + -29312;\n\t// rlwinm r10,r3,6,21,25\n\tctx.r10.u64 = __builtin_rotateleft64(ctx.r3.u32 | (ctx.r3.u64 << 32), 6) & 0x7C0;\n\t// lwzx r11,r9,r11\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r9.u32 + ctx.r11.u32);\n\t// add r11,r11,r10\n\tctx.r11.u64 = ctx.r11.u64 + ctx.r10.u64;\n\t// lbz r10,4(r11)\n\tctx.r10.u64 = PPC_LOAD_U8(ctx.r11.u32 + 4);\n\t// clrlwi. r10,r10,31\n\tctx.r10.u64 = ctx.r10.u32 & 0x1;\n\tctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);\n\t// beq 0x82b86278\n\tif (ctx.cr0.eq) goto loc_82B86278;\n\t// lwz r3,0(r11)\n\tctx.r3.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\nloc_82B862E0:\n\t// addi r1,r1,96\n\tctx.r1.s64 = ctx.r1.s64 + 96;\n\t// lwz r12,-8(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr \n\treturn;\n}"),
    "82B7FDE8": ("LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp", 21256, 28, ["822CA048","822CA048","82B7FD10"], "PPC_FUNC_IMPL(__imp__sub_82B7FDE8) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-8(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);\n\t// std r30,-24(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);\n\t// std r31,-16(r1)\n\tPPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);\n\t// stwu r1,-112(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-112);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// mr r30,r3\n\tctx.r30.u64 = ctx.r3.u64;\n\t// bl 0x822ca048\n\tctx.lr = 0x82B7FE04;\n\tsub_822CA048(ctx, base);\n\t// lis r11,-31967\n\tctx.r11.s64 = -2094989312;\n\t// cmplwi r3,0\n\tctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n\t// addi r31,r11,21008\n\tctx.r31.s64 = ctx.r11.s64 + 21008;\n\t// addi r11,r31,4\n\tctx.r11.s64 = ctx.r31.s64 + 4;\n\t// beq 0x82b7fe1c\n\tif (ctx.cr0.eq) goto loc_82B7FE1C;\n\t// addi r11,r3,12\n\tctx.r11.s64 = ctx.r3.s64 + 12;\nloc_82B7FE1C:\n\t// stw r30,0(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 0, ctx.r30.u32);\n\t// bl 0x822ca048\n\tctx.lr = 0x82B7FE24;\n\tsub_822CA048(ctx, base);\n\t// cmplwi r3,0\n\tctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n\t// mr r7,r31\n\tctx.r7.u64 = ctx.r31.u64;\n\t// beq 0x82b7fe34\n\tif (ctx.cr0.eq) goto loc_82B7FE34;\n\t// addi r7,r3,8\n\tctx.r7.s64 = ctx.r3.s64 + 8;\nloc_82B7FE34:\n\t// mr r3,r30\n\tctx.r3.u64 = ctx.r30.u64;\n\t// bl 0x82b7fd10\n\tctx.lr = 0x82B7FE3C;\n\tsub_82B7FD10(ctx, base);\n\t// stw r3,0(r7)\n\tPPC_STORE_U32(ctx.r7.u32 + 0, ctx.r3.u32);\n\t// addi r1,r1,112\n\tctx.r1.s64 = ctx.r1.s64 + 112;\n\t// lwz r12,-8(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// ld r30,-24(r1)\n\tctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -24);\n\t// ld r31,-16(r1)\n\tctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);\n\t// blr \n\treturn;\n}"),
}
PARAMETERS = {
    "82B7FDB0": "Implicit live r13 CRT thread environment and incoming full-width stack/LR; returns stream error slot address in r3.",
    "82B86228": "Full-width r3 stream index (signed low word), live r13, stack/LR, stream count and block table; returns stream pointer or -1.",
    "82B7FDE8": "Full-width r3 runtime status, live r13, stack/LR and mutable thread records; stores the status and its CRT translation.",
}

def generate(ppc_root: Path | None = None) -> dict:
    if ppc_root is None:
        ppc_root = Path.home() / "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc"
    entries = []
    for address, (source, line, count, direct, expected) in PINS.items():
        path = ROOT / source
        if not path.exists():
            path = ppc_root / Path(source).name
        lines = path.read_text(encoding="utf-8").splitlines()
        if lines[line - 1] != f"PPC_FUNC_IMPL(__imp__sub_{address}) {{":
            raise ValueError(f"{address}: fixed primary source location changed")
        end = next(i for i in range(line, len(lines)) if lines[i] == "}")
        observed = "\n".join(lines[line - 1:end + 1])
        if observed != expected:
            raise ValueError(f"{address}: complete translated body changed")
        instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
        calls = re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", observed)
        if len(instructions) != count or calls != direct:
            raise ValueError(f"{address}: instruction count or direct calls changed")
        if "PPC_CALL_INDIRECT_FUNC" in observed:
            raise ValueError(f"{address}: unexpected indirect PPC call")
        entries.append({
            "address": address, "source": source, "source_line": line,
            "parameters": PARAMETERS[address],
            "instructions": instructions, "direct_calls": calls,
            "cfg": {
                "labels": re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE),
                "branches": re.findall(r"^[ \t]*// (b[^\n]*)$", observed, re.MULTILINE),
            },
            "translated_body": observed,
            "status": "bounded_readable_library",
            "boundary": "Complete selected entry PPC body and own frame with accepted direct lower C++ models; generic lower volatile ABI, external TLS/handler effects, faults, MMIO, concurrency, and runtime remain unverified.",
        })
    return {"schema": "crt-stream-error-v1", "entries": entries}

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(args.ppc_root), indent=2) + "\n"
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")

if __name__ == "__main__":
    main()
