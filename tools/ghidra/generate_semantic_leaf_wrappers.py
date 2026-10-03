"""Generate strong PPC wrappers for the reviewed semantic families."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


SEMANTIC_EXPR = {
    "PreserveR3": "lo::semantic::leaf::PreserveR3(ctx.r3.u64)",
    "ReturnOne": "lo::semantic::leaf::ReturnOne()",
}


def generate(manifest: dict) -> str:
    if manifest.get("schema_version") != 1 or manifest.get("kind") != "exact_leaf_entry_to_shared_semantics":
        raise ValueError("unsupported leaf manifest")
    templates = manifest.get("body_templates")
    if not isinstance(templates, dict) or set(templates) != {"blr", "li_one_blr", "li_one_mr_r8_blr"}:
        raise ValueError("unexpected leaf template set")
    if {name: template.get("semantic") for name, template in templates.items()} != {
        "blr": "PreserveR3", "li_one_blr": "ReturnOne", "li_one_mr_r8_blr": "ReturnOne"
    }:
        raise ValueError("unexpected leaf template semantics")
    entries = manifest.get("entries")
    if not isinstance(entries, list) or not entries:
        raise ValueError("empty leaf manifest")

    lines = [
        "// Generated from leaf_families.json; do not edit.",
        '#include "ppc_context.h"',
        '#include "cpu/semantic_leaf.h"',
        '#include "lo_semantics/leaf_family.h"',
        "",
    ]
    seen = set()
    for entry in entries:
        address = entry.get("address")
        template = entry.get("template")
        if not isinstance(address, str) or not re.fullmatch(r"[0-9A-F]{8}", address) or address in seen:
            raise ValueError(f"invalid or duplicate leaf address: {address}")
        if template not in templates:
            raise ValueError(f"unknown leaf template at {address}: {template}")
        seen.add(address)
        symbol = f"sub_{address}"
        lines.extend([
            f'extern "C" PPC_FUNC(__imp__{symbol});',
            f"PPC_FUNC({symbol})",
            "{",
            "    if (!lo::runtime::semantic_leaf::Enabled())",
            "    {",
            f"        __imp__{symbol}(ctx, base);",
            "        return;",
            "    }",
            f"    ctx.r3.u64 = {SEMANTIC_EXPR[templates[template]['semantic']]};",
            "}",
            "",
        ])
    return "\n".join(lines)


def generate_integer(manifest: dict) -> str:
    if manifest.get("schema_version") != 1:
        raise ValueError("unsupported integer-leaf manifest")
    entries = manifest.get("entries")
    if not isinstance(entries, list) or not entries:
        raise ValueError("empty integer-leaf manifest")
    lines = [
        "// Generated from integer_leaf_families.json; do not edit.",
        '#include "ppc_context.h"',
        '#include "cpu/semantic_integer.h"',
        "",
    ]
    seen = set()
    for entry in entries:
        address = entry.get("address")
        if not isinstance(address, str) or not re.fullmatch(r"[0-9A-F]{8}", address) or address in seen:
            raise ValueError(f"invalid or duplicate integer-leaf address: {address}")
        seen.add(address)
        symbol = f"sub_{address}"
        lines.extend([
            f'extern "C" PPC_FUNC(__imp__{symbol});',
            f"PPC_FUNC({symbol})",
            "{",
            f"    lo::runtime::semantic_integer::Dispatch(0x{address}u, ctx, base, &__imp__{symbol});",
            "}",
            "",
        ])
    return "\n".join(lines)


def generate_accessor(manifest: dict) -> str:
    if manifest.get("schema_version") != 1 or manifest.get("kind") != "fixed_offset_integer_accessor_families":
        raise ValueError("unsupported accessor manifest")
    entries = manifest.get("entries")
    if not isinstance(entries, list) or not entries:
        raise ValueError("empty accessor manifest")
    lines = [
        "// Generated from accessor_families.json; do not edit.",
        '#include "cpu/semantic_accessor.h"',
        "",
    ]
    seen = set()
    for entry in entries:
        address = entry.get("address")
        kind, width = entry.get("kind"), entry.get("width")
        base_register, value_register = entry.get("base_register"), entry.get("value_register")
        displacement = entry.get("displacement")
        if not isinstance(address, str) or not re.fullmatch(r"[0-9A-F]{8}", address) or address in seen:
            raise ValueError(f"invalid or duplicate accessor address: {address}")
        if kind not in ("getter", "setter") or width not in ("Byte", "Halfword", "Word") or \
                base_register not in (3, 4, 5, 6, 13) or \
                type(displacement) is not int or not -32768 <= displacement <= 32767 or \
                (kind == "getter" and value_register is not None) or \
                (kind == "setter" and value_register not in (3, 4, 5)):
            raise ValueError(f"invalid accessor contract at {address}")
        seen.add(address)
        symbol = f"sub_{address}"
        arguments = (f"memory, ctx.r{base_register}.u32, {displacement}, "
                     f"lo::semantic::gpu::IntegerWidth::{width}")
        operation = (f"ctx.r3.u64 = lo::semantic::gpu::ReadFieldWith({arguments});"
                     if kind == "getter" else
                     f"lo::semantic::gpu::WriteFieldWith({arguments}, ctx.r{value_register}.u64);")
        lines.extend([
            f'extern "C" PPC_FUNC(__imp__{symbol});',
            f"PPC_FUNC({symbol})", "{",
            "    if (!lo::runtime::semantic_accessor::Enabled())", "    {",
            f"        __imp__{symbol}(ctx, base);", "        return;", "    }",
            "    lo::runtime::semantic_accessor::NativeAccessorMemory memory(base);",
            f"    {operation}", "}", "",
        ])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    source_group = parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument("--manifest", type=Path)
    source_group.add_argument("--integer-manifest", type=Path)
    source_group.add_argument("--accessor-manifest", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = args.accessor_manifest or args.integer_manifest or args.manifest
    generator = generate_accessor if args.accessor_manifest else generate_integer if args.integer_manifest else generate
    source = generator(json.loads(manifest.read_text(encoding="utf-8")))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.is_file() or args.output.read_text(encoding="utf-8") != source:
        args.output.write_text(source, encoding="utf-8")
    print(f"Generated {source.count('PPC_FUNC(sub_')} semantic wrappers")


if __name__ == "__main__":
    main()
