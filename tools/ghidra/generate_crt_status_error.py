"""Pin the complete 827CA628 status-conversion wrapper body."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp"
LINE = 22674
EXPECTED = 'PPC_FUNC_IMPL(__imp__sub_827CA628) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCRegister temp{};\n\t// mflr r12\n\tctx.r12.u64 = ctx.lr;\n\t// stw r12,-8(r1)\n\tPPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);\n\t// stwu r1,-96(r1)\n\ttemp.u64 = ctx.r1.u64 + uint64_t(-96);\n\tPPC_STORE_U32(temp.u32, ctx.r1.u32);\n\tctx.r1.u64 = temp.u64;\n\t// bl 0x830d9efc\n\tctx.lr = 0x827CA638;\n\t__imp__RtlNtStatusToDosError(ctx, base);\n\t// lwz r11,336(r13)\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r13.u32 + 336);\n\t// cmplwi cr6,r11,0\n\tctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n\t// bne cr6,0x827ca64c\n\tif (!ctx.cr6.eq) goto loc_827CA64C;\n\t// lwz r11,256(r13)\n\tctx.r11.u64 = PPC_LOAD_U32(ctx.r13.u32 + 256);\n\t// stw r3,352(r11)\n\tPPC_STORE_U32(ctx.r11.u32 + 352, ctx.r3.u32);\nloc_827CA64C:\n\t// addi r1,r1,96\n\tctx.r1.s64 = ctx.r1.s64 + 96;\n\t// lwz r12,-8(r1)\n\tctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);\n\t// mtlr r12\n\tctx.lr = ctx.r12.u64;\n\t// blr \n\treturn;\n}'


def generate():
    source = ROOT / SOURCE
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    if lines[LINE - 1] != "PPC_FUNC_IMPL(__imp__sub_827CA628) {":
        raise ValueError("status-error fixed source location changed")
    end = next(i for i in range(LINE, len(lines)) if lines[i] == "}")
    observed = "\n".join(lines[LINE - 1:end + 1])
    if observed != EXPECTED:
        raise ValueError("status-error complete translated body changed")
    instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
    calls = re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", observed)
    imports = re.findall(r"\b__imp__([A-Za-z0-9_]+)\(ctx, base\);", observed)
    if len(instructions) != 13 or calls or imports != ["RtlNtStatusToDosError"]:
        raise ValueError("status-error instructions or dependencies changed")
    return {"schema": "crt-status-error-v1", "entries": [{
        "address": "827CA628", "source": SOURCE, "source_line": LINE,
        "instructions": instructions, "direct_calls": calls, "native_calls": imports,
        "cfg": {
            "labels": re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE),
            "branches": re.findall(r"^[ \t]*// (b[^\n]*)$", observed, re.MULTILINE)},
        "translated_body": observed, "status": "bounded_readable_library",
        "boundary": "Own96 frame, selected mutable native state and ordinary RAM; RtlNtStatusToDosError is explicit. Native internals/unselected state/fault/MMIO/concurrency/runtime unverified."}]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "LostOdysseyRecompSemantics/crt_status_error_families.json")
    args = parser.parse_args()
    payload = json.dumps(generate(), indent=2) + "\n"
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")


if __name__ == "__main__":
    main()
