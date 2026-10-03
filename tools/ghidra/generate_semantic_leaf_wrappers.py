"""Generate strong PPC wrappers for the verified, memory-free leaf families."""

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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    source_group = parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument("--manifest", type=Path)
    source_group.add_argument("--integer-manifest", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = args.integer_manifest or args.manifest
    generator = generate_integer if args.integer_manifest else generate
    source = generator(json.loads(manifest.read_text(encoding="utf-8")))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(source, encoding="utf-8")
    print(f"Generated {source.count('PPC_FUNC(sub_')} semantic wrappers")


if __name__ == "__main__":
    main()
