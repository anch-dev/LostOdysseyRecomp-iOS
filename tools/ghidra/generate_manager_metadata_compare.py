"""Admit one reviewed full UTF16 comparison entry at its recorded PPC location."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path
from generate_instance_vtable_family import write_if_changed

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/manager_metadata_compare_families.json"
SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp"
SOURCE_LINE = 17154
EXPECTED_BODY = 'PPC_FUNC_IMPL(__imp__sub_822971E0) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-8(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);\n\t// stwu r1,-96(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-96);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// cmplwi cr6,r3,0\n\tctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n\t// bne cr6,0x82297228\n\tif (!ctx.cr6.eq) goto loc_82297228;\nloc_822971F4:\n\t// bl 0x82b7fd78\n\tctx.lr = 0x822971F8;\n\tsub_82B7FD78(ctx, base);\n\t// mr r11,r3\n\tctx.r11.u64 = ctx.r3.u64;\n\t// li r10,22\n\tctx.r10.s64 = 22;\n\t// li r7,0\n\tctx.r7.s64 = 0;\n\t// li r6,0\n\tctx.r6.s64 = 0;\n\t// li r5,0\n\tctx.r5.s64 = 0;\n\t// li r4,0\n\tctx.r4.s64 = 0;\n\t// li r3,0\n\tctx.r3.s64 = 0;\n\t// stw r10,0(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 0, ctx.r10.u32);\n\t// bl 0x82b7fec0\n\tctx.lr = 0x8229721C;\n\tsub_82B7FEC0(ctx, base);\n\t// lis r3,32767\n\tctx.r3.s64 = 2147418112;\n\t// ori r3,r3,65535\n\tctx.r3.u64 = ctx.r3.u64 | 65535;\n\t// b 0x82297298\n\tgoto loc_82297298;\nloc_82297228:\n\t// cmplwi cr6,r4,0\n\tctx.cr6.compare<uint32_t>(ctx.r4.u32, 0, ctx.xer);\n\t// beq cr6,0x822971f4\n\tif (ctx.cr6.eq) goto loc_822971F4;\nloc_82297230:\n\t// lhz r11,0(r3)\n\tctx.r11.u64 = PPC_LOAD_U16(ctx.r3.u32 + 0);\n\t// cmplwi cr6,r11,65\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, 65, ctx.xer);\n\t// blt cr6,0x82297248\n\tif (ctx.cr6.lt) goto loc_82297248;\n\t// cmplwi cr6,r11,90\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, 90, ctx.xer);\n\t// addi r10,r11,32\n\tctx.r10.s64 = ctx.r11.s64 + 32;\n\t// ble cr6,0x8229724c\n\tif (!ctx.cr6.gt) goto loc_8229724C;\nloc_82297248:\n\t// mr r10,r11\n\tctx.r10.u64 = ctx.r11.u64;\nloc_8229724C:\n\t// lhz r11,0(r4)\n\tctx.r11.u64 = PPC_LOAD_U16(ctx.r4.u32 + 0);\n\t// clrlwi r9,r10,16\n\tctx.r9.u64 = ctx.r10.u32 & 0xFFFF;\n\t// cmplwi cr6,r11,65\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, 65, ctx.xer);\n\t// blt cr6,0x82297268\n\tif (ctx.cr6.lt) goto loc_82297268;\n\t// cmplwi cr6,r11,90\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, 90, ctx.xer);\n\t// addi r10,r11,32\n\tctx.r10.s64 = ctx.r11.s64 + 32;\n\t// ble cr6,0x8229726c\n\tif (!ctx.cr6.gt) goto loc_8229726C;\nloc_82297268:\n\t// mr r10,r11\n\tctx.r10.u64 = ctx.r11.u64;\nloc_8229726C:\n\t// clrlwi. r11,r9,16\n\tctx.r11.u64 = ctx.r9.u32 & 0xFFFF;\n\tctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n\t// clrlwi r10,r10,16\n\tctx.r10.u64 = ctx.r10.u32 & 0xFFFF;\n\t// addi r3,r3,2\n\tctx.r3.s64 = ctx.r3.s64 + 2;\n\t// addi r4,r4,2\n\tctx.r4.s64 = ctx.r4.s64 + 2;\n\t// beq 0x8229728c\n\tif (ctx.cr0.eq) goto loc_8229728C;\n\t// clrlwi r8,r10,16\n\tctx.r8.u64 = ctx.r10.u32 & 0xFFFF;\n\t// cmplw cr6,r11,r8\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, ctx.r8.u32, ctx.xer);\n\t// beq cr6,0x82297230\n\tif (ctx.cr6.eq) goto loc_82297230;\nloc_8229728C:\n\t// clrlwi r11,r10,16\n\tctx.r11.u64 = ctx.r10.u32 & 0xFFFF;\n\t// clrlwi r10,r9,16\n\tctx.r10.u64 = ctx.r9.u32 & 0xFFFF;\n\t// subf r3,r11,r10\n\tctx.r3.s64 = ctx.r10.s64 - ctx.r11.s64;\nloc_82297298:\n\t// addi r1,r1,96\n\tctx.r1.s64 = ctx.r1.s64 + 96;\n\t// lwz r12,-8(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr\n\treturn;\n}\n'


def reviewed(source_root: Path) -> dict:
    lines = (source_root / SOURCE).read_text(encoding="utf-8").splitlines()
    start = SOURCE_LINE - 1
    if lines[start] != "PPC_FUNC_IMPL(__imp__sub_822971E0) {":
        raise ValueError("comparison source location changed")
    end = next(i for i in range(start + 1, start + 230) if lines[i] == "}")
    body = "\n".join(line.rstrip() for line in lines[start:end + 1]) + "\n"
    if body != EXPECTED_BODY:
        raise ValueError("reviewed comparison body changed")
    instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
    if len(instructions) != 50:
        raise ValueError("comparison instruction count changed")
    calls = re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", body)
    if calls != ["82B7FD78", "82B7FEC0"]:
        raise ValueError("comparison direct calls changed")
    return {"schema_version": 1, "family": "manager_metadata_compare", "entry_count": 1,
        "entries": [{"address": "822971E0", "source": SOURCE, "source_line": SOURCE_LINE,
            "translated_body": body, "instructions": instructions, "direct_calls": calls,
            "parameters": {"frame_bytes": 96, "fold_range": [65, 90], "fold_add": 32,
                "error_code": 22, "invalid_return": "7FFFFFFF"}}],
        "limits": "Own frame, r3-r10/r13, full cursor arithmetic and RAM modeled. Reused lower algorithms exclude generic volatile ABI/frame writes; trap termination, dynamic target internals and faults/MMIO remain external."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path.home() / "ownCloud/Git/LostOdysseyRecomp")
    args = parser.parse_args()
    write_if_changed(MANIFEST, json.dumps(reviewed(args.source_root), indent=2) + "\n")
    print("Verified 1 complete comparison body, 50 instructions and both real lower calls")


if __name__ == "__main__":
    main()
