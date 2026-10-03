"""Admit the exact 823F7B08 named-record body from its fixed PPC source."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_name_record_families.json"
SOURCE = Path("LostOdysseyRecompLib/ppc/ppc_recomp.15.cpp")
ADDRESS = 0x823F7B08
SOURCE_LINE = 10154
EXPECTED_BODY = """PPC_FUNC_IMPL(__imp__sub_823F7B08) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x823F7B10;
	__savegprlr_27(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// mr r30,r3
	ctx.r30.u64 = ctx.r3.u64;
	// mr r29,r4
	ctx.r29.u64 = ctx.r4.u64;
	// mr r28,r6
	ctx.r28.u64 = ctx.r6.u64;
	// bl 0x82296830
	ctx.lr = 0x823F7B24;
	sub_82296830(ctx, base);
	// lis r31,-31951
	ctx.r31.s64 = -2093940736;
	// addi r10,r3,9
	ctx.r10.s64 = ctx.r3.s64 + 9;
	// rlwinm r27,r10,1,0,30
	ctx.r27.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 1) & 0xFFFFFFFE;
	// lwz r11,-18936(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + -18936);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne cr6,0x823f7b44
	if (!ctx.cr6.eq) goto loc_823F7B44;
	// bl 0x827c5f38
	ctx.lr = 0x823F7B40;
	sub_827C5F38(ctx, base);
	// lwz r11,-18936(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + -18936);
loc_823F7B44:
	// lwz r10,0(r11)
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);
	// mr r3,r11
	ctx.r3.u64 = ctx.r11.u64;
	// li r5,8
	ctx.r5.s64 = 8;
	// mr r4,r27
	ctx.r4.u64 = ctx.r27.u64;
	// lwz r11,4(r10)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r10.u32 + 4);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl\x20
	ctx.lr = 0x823F7B60;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// li r11,0
	ctx.r11.s64 = 0;
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// addi r3,r31,16
	ctx.r3.s64 = ctx.r31.s64 + 16;
	// stw r29,0(r31)
	PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r29.u32);
	// std r11,4(r31)
	PPC_STORE_U64(ctx.r31.u32 + 4, ctx.r11.u64);
	// stw r28,12(r31)
	PPC_STORE_U32(ctx.r31.u32 + 12, ctx.r28.u32);
	// bl 0x8230bac0
	ctx.lr = 0x823F7B80;
	sub_8230BAC0(ctx, base);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// addi r1,r1,128
	ctx.r1.s64 = ctx.r1.s64 + 128;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
}
"""


def generate() -> dict:
    source = ROOT / SOURCE
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    if lines[SOURCE_LINE - 1] != "PPC_FUNC_IMPL(__imp__sub_823F7B08) {":
        raise ValueError("823F7B08 moved from fixed PPC source line")
    tail = "\n".join(lines[SOURCE_LINE - 1:])
    observed = tail[:tail.index("\n}\n") + 2]
    if observed != EXPECTED_BODY.rstrip("\n"):
        raise ValueError("823F7B08 full translated body differs from admitted body")
    instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
    expected_calls = ["82296830", "827c5f38", "8230bac0"]
    direct_calls = re.findall(r"// bl 0x([0-9a-f]+)", observed)
    if [x.lower() for x in direct_calls if x.lower() not in ("82b7a6e4",)] != expected_calls:
        raise ValueError("823F7B08 direct-call sequence changed")
    if instructions.count("bctrl ") != 1 or instructions.count("bne cr6,0x823f7b44") != 1:
        raise ValueError("823F7B08 virtual call or branch changed")
    if re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE) != ["823F7B44"]:
        raise ValueError("823F7B08 CFG labels changed")
    return {
        "schema": "metadata-name-record-v1",
        "entries": [{
            "address": "823F7B08",
            "source": str(SOURCE).replace("\\", "/"),
            "source_line": SOURCE_LINE,
            "shape": "utf16-name-record-manager-allocation",
            "instructions": instructions,
            "cfg": {"branch": "bne cr6,0x823f7b44", "join": "823F7B44"},
            "calls": ["82296830", "827C5F38", "manager_vtable_plus_4", "8230BAC0"],
            "manager_global": "8330B608",
            "frame_size": 128,
            "save_first": 27,
            "fields": [{"offset": 0, "width": 32, "source": "live_r29"},
                       {"offset": 4, "width": 64, "source": "zero"},
                       {"offset": 12, "width": 32, "source": "live_r28"},
                       {"offset": 16, "width": 16, "source": "utf16_until_zero_from_live_r30"}],
            "allocation": {"length_add": 9, "byte_scale": 2, "alignment": 8},
            "translated_body": observed,
            "status": "bounded_readable_library",
            "boundary": "manager virtual allocation is an explicit service; ManagerInitServices retains its lower ABI boundary",
        }],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(), indent=2) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")


if __name__ == "__main__":
    main()
