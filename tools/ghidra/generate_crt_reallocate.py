"""Pin the complete original PPC control flow of CRT realloc 823ACAD8."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/crt_reallocate_families.json"
ADDRESS = "823ACAD8"
SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp"
LINE = 26875
EXPECTED = "PPC_FUNC_IMPL(__imp__sub_823ACAD8) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// bl 0x82b7a6e4\n\tctx.lr = 0x823ACAE0;\n\t__savegprlr_27(ctx, base);\n\t// stwu r1,-128(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-128);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// mr r28,r3\n\tctx.r28.u64 = ctx.r3.u64;\n\t// mr r31,r4\n\tctx.r31.u64 = ctx.r4.u64;\n\t// cmplwi cr6,r28,0\n\tctx.cr6.compare<uint32_t>(ctx.r28.u32, 0, ctx.xer);\n\t// bne cr6,0x823acb00\n\tif (!ctx.cr6.eq) goto loc_823ACB00;\n\t// mr r3,r31\n\tctx.r3.u64 = ctx.r31.u64;\n\t// bl 0x823acbd0\n\tctx.lr = 0x823ACAFC;\n\tsub_823ACBD0(ctx, base);\n\t// b 0x823acb8c\n\tgoto loc_823ACB8C;\nloc_823ACB00:\n\t// cmplwi cr6,r31,0\n\tctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);\n\t// bne cr6,0x823acb14\n\tif (!ctx.cr6.eq) goto loc_823ACB14;\n\t// mr r3,r28\n\tctx.r3.u64 = ctx.r28.u64;\n\t// bl 0x823addc0\n\tctx.lr = 0x823ACB10;\n\tsub_823ADDC0(ctx, base);\n\t// b 0x823acb88\n\tgoto loc_823ACB88;\nloc_823ACB14:\n\t// li r29,-4096\n\tctx.r29.s64 = -4096;\n\t// cmplw cr6,r31,r29\n\tctx.cr6.compare<uint32_t>(ctx.r31.u32, ctx.r29.u32, ctx.xer);\n\t// bgt cr6,0x823acb70\n\tif (ctx.cr6.gt) goto loc_823ACB70;\n\t// lis r27,-31955\n\tctx.r27.s64 = -2094202880;\nloc_823ACB24:\n\t// cmplwi cr6,r31,0\n\tctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);\n\t// bne cr6,0x823acb30\n\tif (!ctx.cr6.eq) goto loc_823ACB30;\n\t// li r31,1\n\tctx.r31.s64 = 1;\nloc_823ACB30:\n\t// bl 0x823acc98\n\tctx.lr = 0x823ACB34;\n\tsub_823ACC98(ctx, base);\n\t// li r4,0\n\tctx.r4.s64 = 0;\n\t// mr r5,r28\n\tctx.r5.u64 = ctx.r28.u64;\n\t// mr r6,r31\n\tctx.r6.u64 = ctx.r31.u64;\n\t// bl 0x827ccf80\n\tctx.lr = 0x823ACB44;\n\tsub_827CCF80(ctx, base);\n\t// mr. r30,r3\n\tctx.r30.u64 = ctx.r3.u64;\n\tctx.cr0.compare<int32_t>(ctx.r30.s32, 0, ctx.xer);\n\t// bne 0x823acbc4\n\tif (!ctx.cr0.eq) goto loc_823ACBC4;\n\t// lwz r11,15084(r27)\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r27.u32 + 15084);\n\t// cmpwi cr6,r11,0\n\tctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n\t// beq cr6,0x823acbb0\n\tif (ctx.cr6.eq) goto loc_823ACBB0;\n\t// mr r3,r31\n\tctx.r3.u64 = ctx.r31.u64;\n\t// bl 0x82b7fe68\n\tctx.lr = 0x823ACB60;\n\tsub_82B7FE68(ctx, base);\n\t// cmpwi r3,0\n\tctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n\t// beq 0x823acb94\n\tif (ctx.cr0.eq) goto loc_823ACB94;\n\t// cmplw cr6,r31,r29\n\tctx.cr6.compare<uint32_t>(ctx.r31.u32, ctx.r29.u32, ctx.xer);\n\t// ble cr6,0x823acb24\n\tif (!ctx.cr6.gt) goto loc_823ACB24;\nloc_823ACB70:\n\t// mr r3,r31\n\tctx.r3.u64 = ctx.r31.u64;\n\t// bl 0x82b7fe68\n\tctx.lr = 0x823ACB78;\n\tsub_82B7FE68(ctx, base);\n\t// bl 0x82b7fd78\n\tctx.lr = 0x823ACB7C;\n\tsub_82B7FD78(ctx, base);\n\t// mr r11,r3\n\tctx.r11.u64 = ctx.r3.u64;\n\t// li r10,12\n\tctx.r10.s64 = 12;\n\t// stw r10,0(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 0, ctx.r10.u32);\nloc_823ACB88:\n\t// li r3,0\n\tctx.r3.s64 = 0;\nloc_823ACB8C:\n\t// addi r1,r1,128\n\tctx.r1.s64 = ctx.r1.s64 + 128;\n\t// b 0x82b7a734\n\t__restgprlr_27(ctx, base);\n\treturn;\nloc_823ACB94:\n\t// bl 0x82b7fd78\n\tctx.lr = 0x823ACB98;\n\tsub_82B7FD78(ctx, base);\n\t// mr r31,r3\n\tctx.r31.u64 = ctx.r3.u64;\n\t// bl 0x822ca100\n\tctx.lr = 0x823ACBA0;\n\tsub_822CA100(ctx, base);\n\t// bl 0x82b7fd10\n\tctx.lr = 0x823ACBA4;\n\tsub_82B7FD10(ctx, base);\n\t// mr r11,r3\n\tctx.r11.u64 = ctx.r3.u64;\n\t// stw r11,0(r31)\n\tPPC_STORE_U32(ctx.r31.u32 + 0, ctx.r11.u32);\n\t// b 0x823acb88\n\tgoto loc_823ACB88;\nloc_823ACBB0:\n\t// bl 0x82b7fd78\n\tctx.lr = 0x823ACBB4;\n\tsub_82B7FD78(ctx, base);\n\t// mr r31,r3\n\tctx.r31.u64 = ctx.r3.u64;\n\t// bl 0x822ca100\n\tctx.lr = 0x823ACBBC;\n\tsub_822CA100(ctx, base);\n\t// bl 0x82b7fd10\n\tctx.lr = 0x823ACBC0;\n\tsub_82B7FD10(ctx, base);\n\t// stw r3,0(r31)\n\tPPC_STORE_U32(ctx.r31.u32 + 0, ctx.r3.u32);\nloc_823ACBC4:\n\t// mr r3,r30\n\tctx.r3.u64 = ctx.r30.u64;\n\t// b 0x823acb8c\n\tgoto loc_823ACB8C;\n}"
DIRECT = ["823ACBD0", "823ADDC0", "823ACC98", "827CCF80",
    "82B7FE68", "82B7FE68", "82B7FD78", "82B7FD78", "822CA100",
    "82B7FD10", "82B7FD78", "822CA100", "82B7FD10"]


def generate() -> dict:
    source = ROOT / SOURCE
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    if lines[LINE - 1] != f"PPC_FUNC_IMPL(__imp__sub_{ADDRESS}) {{":
        raise ValueError("CRT realloc fixed source location changed")
    end = next(i for i in range(LINE, len(lines)) if lines[i] == "}")
    observed = "\n".join(lines[LINE - 1:end + 1])
    if observed != EXPECTED:
        raise ValueError("CRT realloc complete translated body changed")
    instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
    if len(instructions) != 61:
        raise ValueError("CRT realloc instruction count changed")
    direct = re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", observed)
    if direct != DIRECT or "PPC_CALL_INDIRECT_FUNC" in observed:
        raise ValueError("CRT realloc direct or indirect call sequence changed")
    if observed.count("__savegprlr_27(ctx, base);") != 1 or \
            observed.count("__restgprlr_27(ctx, base);") != 1:
        raise ValueError("CRT realloc save/restore ABI changed")
    return {"schema": "crt-reallocate-v1", "entries": [{
        "address": ADDRESS, "source": SOURCE, "source_line": LINE,
        "instructions": instructions, "direct_calls": direct,
        "abi_helpers": ["82B7A6E4", "82B7A734"],
        "cfg": {
            "labels": re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE),
            "branches": re.findall(r"^[ \t]*// (b[^\n]*)$", observed, re.MULTILINE),
        },
        "translated_body": observed,
        "status": "bounded_readable_library",
        "boundary": "Whole CRT caller with accepted PPC child models; selected registers, own frame and ordinary RAM; generic lower volatile ABI, native service internals, faults, MMIO, concurrency and runtime unverified."
    }]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(), indent=2) + "\n"
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")


if __name__ == "__main__":
    main()
