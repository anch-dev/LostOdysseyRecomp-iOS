"""Admit two exact manager registration foundations from fixed PPC locations."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path
from generate_instance_vtable_family import write_if_changed

ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "manager_object_registration_families.json"
SPECS = {
    "8232D318": ("LostOdysseyRecompLib/ppc/ppc_recomp.6.cpp", 15193,
                  "utf16_copy_padded"),
    "82376EE8": ("LostOdysseyRecompLib/ppc/ppc_recomp.9.cpp", 21262,
                  "lazy_registration_object"),
}
EXPECTED = {
    "8232D318": """PPC_FUNC_IMPL(__imp__sub_8232D318) {
	PPC_FUNC_PROLOGUE();
	// mr r11,r3
	ctx.r11.u64 = ctx.r3.u64;
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r5.u32, 0, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
loc_8232D324:
	// lhz r10,0(r4)
	ctx.r10.u64 = PPC_LOAD_U16(ctx.r4.u32 + 0);
	// addi r4,r4,2
	ctx.r4.s64 = ctx.r4.s64 + 2;
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// sth r10,0(r11)
	PPC_STORE_U16(ctx.r11.u32 + 0, ctx.r10.u16);
	// addi r11,r11,2
	ctx.r11.s64 = ctx.r11.s64 + 2;
	// beq 0x8232d344
	if (ctx.cr0.eq) goto loc_8232D344;
	// addic. r5,r5,-1
	ctx.xer.ca = ctx.r5.u32 > 0;
	ctx.r5.s64 = ctx.r5.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r5.s32, 0, ctx.xer);
	// bne 0x8232d324
	if (!ctx.cr0.eq) goto loc_8232D324;
loc_8232D344:
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r5.u32, 0, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// addic. r10,r5,-1
	ctx.xer.ca = ctx.r5.u32 > 0;
	ctx.r10.s64 = ctx.r5.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr
	if (ctx.cr0.eq) return;
	// li r9,0
	ctx.r9.s64 = 0;
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// beqlr
	if (ctx.cr0.eq) return;
	// mtctr r10
	ctx.ctr.u64 = ctx.r10.u64;
loc_8232D364:
	// sth r9,0(r11)
	PPC_STORE_U16(ctx.r11.u32 + 0, ctx.r9.u16);
	// addi r11,r11,2
	ctx.r11.s64 = ctx.r11.s64 + 2;
	// bdnz 0x8232d364
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_8232D364;
	// blr
	return;
}
""",
    "82376EE8": """PPC_FUNC_IMPL(__imp__sub_82376EE8) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// lis r31,-31951
	ctx.r31.s64 = -2093940736;
	// lwz r3,24444(r31)
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 24444);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// bne cr6,0x82376f20
	if (!ctx.cr6.eq) goto loc_82376F20;
	// lis r11,-32231
	ctx.r11.s64 = -2112290816;
	// addi r3,r11,-15856
	ctx.r3.s64 = ctx.r11.s64 + -15856;
	// bl 0x82408438
	ctx.lr = 0x82376F14;
	sub_82408438(ctx, base);
	// stw r3,24444(r31)
	PPC_STORE_U32(ctx.r31.u32 + 24444, ctx.r3.u32);
	// bl 0x824084f0
	ctx.lr = 0x82376F1C;
	sub_824084F0(ctx, base);
	// lwz r3,24444(r31)
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 24444);
loc_82376F20:
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);
	// blr
	return;
}
""",
}

def normalize(body: str) -> list[str]:
    return [line.strip() for line in body.splitlines() if line.strip()]

def source_body(path: Path, line: int) -> str:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    start = line - 1
    if start < 0 or start >= len(lines):
        raise ValueError(f"source line moved: {path}:{line}")
    end = start + 1
    while end < len(lines) and lines[end].strip() != "}":
        if lines[end].startswith("PPC_FUNC_IMPL("):
            raise ValueError("unterminated original PPC body")
        end += 1
    if end == len(lines):
        raise ValueError("unterminated original PPC body")
    return "".join(lines[start:end + 1])

def generate() -> dict:
    entries = []
    for address, (source, line, kind) in SPECS.items():
        cached = (ROOT / "out/function-inventory" /
                  f"manager-object-registration-{address}.cpp").read_text(encoding="utf-8")
        source_path = ROOT / source
        if not source_path.exists():
            source_path = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / source
        original = source_body(source_path, line) if source_path.exists() else cached
        expected = EXPECTED[address]
        if normalize(cached) != normalize(expected) or normalize(original) != normalize(expected):
            raise ValueError(f"{address}: complete translated body or CFG changed")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", original, re.M)
        calls = re.findall(r"^\s*// bl 0x([0-9a-f]+)$", original, re.M)
        allowed_calls = ["82408438", "824084f0"] if address == "82376EE8" else []
        if calls != allowed_calls:
            raise ValueError(f"{address}: direct-call targets changed")
        entries.append({"address": address, "source": source,
                        "source_line": line, "kind": kind,
                        "instruction_sequence": instructions,
                        "translated_body": original,
                        "cfg": [part for part in instructions if part.startswith(("b", "mtctr"))],
                        "direct_calls": calls,
                        "bounded_status": "recovered_with_reused_lower_semantics" if calls else "recovered"})
    return {"schema_version": 1, "entry_count": 2, "entries": entries,
            "open_parents": ["823FFDD8", "8256F6B8"],
            "bounded_effects": "82376EE8 composes already recovered constructor and secondary registration helpers; external callbacks retain full reconstructed stack address. GuestMemory covers ordinary RAM and own frame LR/r31. Lower helper generic ABI/volatile state and dynamic callback implementations are explicit boundaries. 8232D318 copies ordered UTF16 through GuestMemory; volatile CR/XER/CTR/r10/r11 excluded."}

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    encoded = json.dumps(generate(), indent=2) + "\n"
    if args.write:
        write_if_changed(MANIFEST, encoded)
    elif MANIFEST.read_text(encoding="utf-8") != encoded:
        raise ValueError("manager object registration manifest changed")
    print("PASS manager object registration 2 exact bodies/CFG/calls")

if __name__ == "__main__":
    main()
