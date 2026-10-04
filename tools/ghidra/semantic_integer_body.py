"""Syntax adapter for explicitly pinned integer PPC bodies.

This does not recover or approve semantics. A selected implementation supplies
its Context, Base, memory accesses and actual direct/native call composition;
the original-body oracle still compiles the authoritative pin unchanged.
"""
from __future__ import annotations

import re


def translate_body(entry: dict) -> str:
    address = entry["address"]
    original = entry["translated_body"]
    marker = f"PPC_FUNC_IMPL(__imp__sub_{address}) {{"
    if original.count(marker) != 1:
        raise ValueError(f"missing unique pinned entry: {address}")
    result = original.replace(
        marker, f"void Body_{address}(Context& ctx, Base& base) {{", 1
    )
    result = result.replace("\tPPC_FUNC_PROLOGUE();\n", "")
    result = result.replace("PPCRegister", "PpcRegister")
    result = re.sub(r"ctx\.r(\d+)\b", r"ctx.r[\1]", result)
    result = result.replace("__builtin_clz", "std::countl_zero")
    result = result.replace("__builtin_rotateleft64", "std::rotl")
    result = result.replace("__builtin_rotateleft32", "std::rotl")
    # Integer destinations retain the low byte; both operands still evaluate.
    # This also avoids /WX boolean-bitwise warnings in the original spelling.
    result = re.sub(
        r"(\b(?:ctx\.xer\.ca|temp\.u8)\s*=\s*)(\([^\n;]+?\)) ([&|]) (\([^\n;]+?\));",
        lambda m: f"{m[1]}std::uint8_t({m[2]}) {m[3]} std::uint8_t({m[4]});",
        result,
    )
    # Comments are the authoritative instruction sequence. Adapt syntax only.
    instructions = re.findall(r"^\s*// (.*)$", result, re.M)
    if instructions != entry["instructions"]:
        raise ValueError(f"instruction sequence changed: {address}")
    return result
