"""Fold 446 ordered register-load/call pairs into a checked readable data table."""
import argparse
import json
import re
from pathlib import Path
from semantic_batch import ROOT, extract_originals

ENTRY = {"address": "823F4700", "generated_ppc_path": "ppc_recomp.15.cpp", "line": 2522}
MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_name_registry_families.json"
TABLE = ROOT / "LostOdysseyRecompSemantics/src/metadata_name_registry_entries.inc"
EXPECTED_PREFIX = 'PPC_FUNC_IMPL(__imp__sub_823F4700) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-8(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);\n\t// stwu r1,-96(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-96);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// lis r9,-31945\n\tctx.r9.s64 = -2093547520;\n\t// li r7,0\n\tctx.r7.s64 = 0;\n\t// lwz r10,6808(r9)\n\tctx.r10.u64 = PPC_LOAD_U32(ctx.r9.u32 + 6808);\n\t// clrlwi r11,r10,31\n\tctx.r11.u64 = ctx.r10.u32 & 0x1;\n\t// cmplwi cr6,r11,0\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n\t// bne cr6,0x823f477c\n\tif (!ctx.cr6.eq) goto loc_823F477C;\n\t// lis r11,-31953\n\tctx.r11.s64 = -2094071808;\n\t// ori r10,r10,1\n\tctx.r10.u64 = ctx.r10.u64 | 1;\n\t// addi r8,r11,-7832\n\tctx.r8.s64 = ctx.r11.s64 + -7832;\n\t// lis r11,1217\n\tctx.r11.s64 = 79757312;\n\t// ori r6,r11,7607\n\tctx.r6.u64 = ctx.r11.u64 | 7607;\n\t// stw r10,6808(r9)\n\tPPC_STORE_U32(ctx.r9.u32 + 6808, ctx.r10.u32);\n\t// mr r10,r7\n\tctx.r10.u64 = ctx.r7.u64;\nloc_823F4740:\n\t// rlwinm r11,r10,24,0,7\n\tctx.r11.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 24) & 0xFF000000;\n\t// li r9,8\n\tctx.r9.s64 = 8;\nloc_823F4748:\n\t// rlwinm r5,r11,0,0,0\n\tctx.r5.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x80000000;\n\t// rlwinm r11,r11,1,0,30\n\tctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 1) & 0xFFFFFFFE;\n\t// cmplwi cr6,r5,0\n\tctx.cr6.compare<uint32_t>(ctx.r5.u32, 0, ctx.xer);\n\t// beq cr6,0x823f475c\n\tif (ctx.cr6.eq) goto loc_823F475C;\n\t// xor r11,r11,r6\n\tctx.r11.u64 = ctx.r11.u64 ^ ctx.r6.u64;\nloc_823F475C:\n\t// addi r9,r9,-1\n\tctx.r9.s64 = ctx.r9.s64 + -1;\n\t// cmplwi cr6,r9,0\n\tctx.cr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);\n\t// bne cr6,0x823f4748\n\tif (!ctx.cr6.eq) goto loc_823F4748;\n\t// addi r10,r10,1\n\tctx.r10.s64 = ctx.r10.s64 + 1;\n\t// stw r11,0(r8)\n\tPPC_STORE_U32(ctx.r8.u32 + 0, ctx.r11.u32);\n\t// addi r8,r8,4\n\tctx.r8.s64 = ctx.r8.s64 + 4;\n\t// cmplwi cr6,r10,256\n\tctx.cr6.compare<uint32_t>(ctx.r10.u32, 256, ctx.xer);\n\t// blt cr6,0x823f4740\n\tif (ctx.cr6.lt) goto loc_823F4740;\nloc_823F477C:\n\t// lis r11,-31945\n\tctx.r11.s64 = -2093547520;\n\t// lis r8,-31964\n\tctx.r8.s64 = -2094792704;\n\t// addi r11,r11,-28464\n\tctx.r11.s64 = ctx.r11.s64 + -28464;\n\t// li r9,1\n\tctx.r9.s64 = 1;\n\t// lis r10,-31953\n\tctx.r10.s64 = -2094071808;\n\t// addi r10,r10,-6808\n\tctx.r10.s64 = ctx.r10.s64 + -6808;\n\t// stw r7,0(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 0, ctx.r7.u32);\n\t// stw r7,4(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 4, ctx.r7.u32);\n\t// stw r9,25184(r8)\n\tPPC_STORE_U32(ctx.r8.u32 + 25184, ctx.r9.u32);\n\t// mr r8,r7\n\tctx.r8.u64 = ctx.r7.u64;\n\t// stw r7,8(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 8, ctx.r7.u32);\n\t// li r9,4096\n\tctx.r9.s64 = 4096;\n\t// mtctr r9\n\tctx.ctr.u64 = ctx.r9.u64;\nloc_823F47B0:\n\t// stw r8,0(r10)\n\tPPC_STORE_U32(ctx.r10.u32 + 0, ctx.r8.u32);\n\t// addi r10,r10,4\n\tctx.r10.s64 = ctx.r10.s64 + 4;\n\t// bdnz 0x823f47b0\n\t--ctx.ctr.u64;\n\tif (ctx.ctr.u32 != 0) goto loc_823F47B0;\n'
EXPECTED_SUFFIX = '\t// addi r1,r1,96\n\tctx.r1.s64 = ctx.r1.s64 + 96;\n\t// lwz r12,-8(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr\n\treturn;\n}\n'
ORDERS = [
    ("lis", "r6", "name", "r5", "id"),
    ("lis", "name", "r6", "r5", "id"),
    ("lis", "r6", "r5", "id", "name"),
    ("lis", "r6", "r5", "name", "id"),
]


def row_body(name, identifier, order, ordinal):
    upper = ((name + 0x8000) >> 16) & 0xFFFF
    signed_upper = upper if upper < 0x8000 else upper - 0x10000
    low = name & 0xFFFF
    signed_low = low if low < 0x8000 else low - 0x10000
    pieces = {
        "lis": f"\t// lis r11,{signed_upper}\n\tctx.r11.s64 = {signed_upper * 65536};\n",
        "r6": "\t// li r6,0\n\tctx.r6.s64 = 0;\n",
        "r5": "\t// li r5,0\n\tctx.r5.s64 = 0;\n",
        "name": f"\t// addi r3,r11,{signed_low}\n\tctx.r3.s64 = ctx.r11.s64 + {signed_low};\n",
        "id": f"\t// li r4,{identifier}\n\tctx.r4.s64 = {identifier};\n",
    }
    return "".join(pieces[k] for k in ORDERS[order]) + (
        f"\t// bl 0x823f7b08\n\tctx.lr = 0x{0x823F47D4 + ordinal * 28:X};\n\tsub_823F7B08(ctx, base);\n"
        f"\t// bl 0x823f44e8\n\tctx.lr = 0x{0x823F47D8 + ordinal * 28:X};\n\tsub_823F44E8(ctx, base);\n")


def reviewed(ppc_root):
    raw = extract_originals([ENTRY], ppc_root).decode().replace("\r\n", "\n")
    body = "\n".join(line.rstrip() for line in raw.splitlines()) + "\n"
    prefix = EXPECTED_PREFIX
    suffix = EXPECTED_SUFFIX
    if not body.startswith(prefix) or not body.endswith(suffix):
        raise ValueError("registry initialization/frame CFG changed")
    middle = body[len(prefix):-len(suffix)]
    blocks = re.findall(r".*?\tsub_823F44E8\(ctx, base\);\n", middle, re.S)
    if len(blocks) != 446 or "".join(blocks) != middle:
        raise ValueError("registry is not exactly 446 complete ordered pairs")
    rows = []
    for ordinal, block in enumerate(blocks):
        upper = int(re.search(r"// lis r11,(-?\d+)", block)[1])
        lower = int(re.search(r"// addi r3,r11,(-?\d+)", block)[1])
        identifier = int(re.search(r"// li r4,(\d+)", block)[1])
        name = (upper * 65536 + lower) & 0xFFFFFFFF
        candidates = [i for i in range(len(ORDERS)) if row_body(name, identifier, i, ordinal) == block]
        if len(candidates) != 1:
            raise ValueError(f"unreviewed full parameter/call translation: row {ordinal}")
        rows.append({"name": f"{name:08X}", "id": identifier, "load_order": candidates[0],
            "record_lr": f"{0x823F47D4 + ordinal * 28:08X}",
            "insert_lr": f"{0x823F47D8 + ordinal * 28:08X}"})
    instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
    if len(instructions) != 3173 or len({r["name"] for r in rows}) != 446 or len({r["id"] for r in rows}) != 446:
        raise ValueError("registry instruction/record identity count changed")
    table = "// Ordered original name pointers and ids; these are data, not entry points.\n" + "".join(
        f"    {{0x{r['name']}u, {r['id']}u}},\n" for r in rows)
    manifest = {"schema_version": 1, "family": "metadata_name_registry", "entry_count": 1,
        "entries": [{**ENTRY, "instruction_sequence": instructions, "translated_body": body,
            "record_count": 446, "records": rows,
            "direct_calls": {"823F7B08": 446, "823F44E8": 446}}],
        "limits": "The 446 records are one real entry. Existing recovered callees and explicit dynamic allocator/resize contracts compose actual algorithms. Selected own-frame/nonvolatile and full residual r3 are modeled; generic lower volatile ABI, target internals, faults/MMIO/concurrency remain open."}
    return manifest, table


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path.home() / "ownCloud/Git/LostOdysseyRecomp")
    parser.add_argument("--write-reviewed-baseline", action="store_true")
    args = parser.parse_args()
    manifest, table = reviewed(args.source_root / "LostOdysseyRecompLib/ppc")
    rendered = json.dumps(manifest, indent=2) + "\n"
    if args.write_reviewed_baseline:
        MANIFEST.write_text(rendered, encoding="utf-8")
        TABLE.write_text(table, encoding="utf-8")
    elif MANIFEST.read_text(encoding="utf-8") != rendered or TABLE.read_text(encoding="utf-8") != table:
        raise ValueError("registry manifest/readable data table changed")
    print("Verified complete 3173-instruction registry, 446 ordered pairs and four parameter-load templates")


if __name__ == "__main__":
    main()
