"""Admit both fixed PPC bodies in the CRT indexed lock exit family."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/crt_stream_index_unlock_families.json"
SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.176.cpp"
SPECS = (("82B819C8", 4241, 5, [], ["RtlLeaveCriticalSection"]),
         ("82B863B8", 15992, 13, ["82B819C8"], []))
EXPECTED_BODIES = {
    "82B819C8": "PPC_FUNC_IMPL(__imp__sub_82B819C8) {\n\tPPC_FUNC_PROLOGUE();\n\t// lis r11,-31967\n\tctx.r11.s64 = -2094989312;\n\t// rlwinm r10,r3,3,0,28\n\tctx.r10.u64 = __builtin_rotateleft64(ctx.r3.u32 | (ctx.r3.u64 << 32), 3) & 0xFFFFFFF8;\n\t// addi r11,r11,21336\n\tctx.r11.s64 = ctx.r11.s64 + 21336;\n\t// lwzx r3,r10,r11\n\tctx.r3.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r11.u32);\n\t// b 0x830d9c7c\n\t__imp__RtlLeaveCriticalSection(ctx, base);\n\treturn;\n}",
    "82B863B8": "PPC_FUNC_IMPL(__imp__sub_82B863B8) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-8(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);\n\t// stwu r1,-96(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-96);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// li r3,10\n\tctx.r3.s64 = 10;\n\t// bl 0x82b819c8\n\tctx.lr = 0x82B863CC;\n\tsub_82B819C8(ctx, base);\n\t// lis r11,-31944\n\tctx.r11.s64 = -2093481984;\n\t// addi r11,r11,-29312\n\tctx.r11.s64 = ctx.r11.s64 + -29312;\n\t// lwz r3,148(r31)\n\tctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 148);\n\t// lwz r29,80(r31)\n\tctx.r29.u64 = PPC_LOAD_U32(ctx.r31.u32 + 80);\n\t// lwz r1,0(r1)\n\tctx.r1.u64 = PPC_LOAD_U32(ctx.r1.u32 + 0);\n\t// lwz r12,-8(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr \n\treturn;\n}"
}


def generate(ppc_root: Path | None = None) -> dict:
    entries = []
    if ppc_root is None:
        ppc_root = ROOT / "LostOdysseyRecompLib/ppc"
    source = ppc_root / "ppc_recomp.176.cpp"
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    for address, line, count, direct, native in SPECS:
        start = line - 1
        if lines[start] != f"PPC_FUNC_IMPL(__imp__sub_{address}) {{":
            raise ValueError(f"CRT stream body moved: {address}")
        end = next(i for i in range(start + 1, len(lines)) if lines[i] == "}")
        body = "\n".join(lines[start:end + 1])
        instructions = [item.rstrip() for item in
            re.findall(r"^[ \t]*// (.+)$", body, re.M)]
        actual_direct = re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", body)
        actual_native = re.findall(r"\b__imp__([A-Za-z0-9_]+)\(ctx, base\);", body)
        if (body != EXPECTED_BODIES[address] or len(instructions) != count or
                actual_direct != direct or
                actual_native != native):
            raise ValueError(f"complete CRT stream body/calls changed: {address}")
        entries.append({"address": address, "source": SOURCE, "source_line": line,
            "instructions": instructions, "direct_calls": direct,
            "native_calls": native,
            "cfg": {"labels": re.findall(r"^loc_([0-9A-F]+):", body, re.M),
                "branches": [item for item in instructions if item.startswith("b") and
                             ("0x" in item or item == "blr")]},
            "translated_body": body, "status": "bounded_readable_library",
            "boundary": "Complete selected registers and ordinary RAM, real tail native callback and live wrapper epilogue; other volatile registers, faults, MMIO, concurrency and runtime unverified."})
    result = {"schema": "crt-stream-index-unlock-v1", "entries": entries}
    if MANIFEST.exists() and json.loads(MANIFEST.read_text(encoding="utf-8")) != result:
        raise ValueError("CRT stream indexed-lock manifest differs from fixed PPC source")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=None)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(args.ppc_root), indent=2) + "\n"
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")


if __name__ == "__main__":
    main()
