"""Compare 827CBCA8 against its complete fixed original PPC body."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

from generate_heap_block_resize import MANIFEST, ROOT, generate

sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402


def abi_helper(ppc_root: Path, name: str, line: int, restore: bool) -> bytes:
    lines = (ppc_root / "ppc_recomp.175.cpp").read_bytes().splitlines(keepends=True)
    start = line - 1
    if lines[start].strip() != f"PPC_FUNC_IMPL(__imp____{name}) {{".encode():
        raise ValueError(f"ABI helper moved: {name}")
    end = next(i for i in range(start + 1, start + 40) if lines[i].strip() == b"}")
    body = b"".join(lines[start:end + 1])
    operation = "ld" if restore else "std"
    expected = [f"{operation} r{reg},-{8 * (33 - reg)}(r1)"
                for reg in range(22, 32)]
    expected += (["lwz r12,-8(r1)", "mtlr r12", "blr"] if restore
                 else ["stw r12,-8(r1)", "blr"])
    observed = [x.decode("ascii") for x in re.findall(
        rb"^\s*//\s*(.*?)\s*$", body, re.M)]
    if observed != expected:
        raise ValueError(f"ABI helper changed: {name}")
    return body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-heap-block-resize-tests")
    args = parser.parse_args()
    manifest = generate(args.ppc_root)
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != manifest:
        raise ValueError("heap resize manifest changed")
    prelude = b"""
PPC_FUNC(__savegprlr_22); PPC_FUNC(__restgprlr_22);
void OriginalExtend(PPCContext&, std::uint8_t*);
void OriginalCoalesce(PPCContext&, std::uint8_t*);
void OriginalInsert(PPCContext&, std::uint8_t*);
void OriginalFill(PPCContext&, std::uint8_t*);
void OriginalCompare(PPCContext&, std::uint8_t*);
#define sub_827CB778(ctx, base) OriginalExtend(ctx, base)
#define sub_823AE108(ctx, base) OriginalCoalesce(ctx, base)
#define sub_827CBA60(ctx, base) OriginalInsert(ctx, base)
#define sub_82B7BC40(ctx, base) OriginalFill(ctx, base)
#define __imp__RtlCompareMemoryUlong(ctx, base) OriginalCompare(ctx, base)
"""
    wrappers = b"""
PPC_FUNC(__savegprlr_22) { __imp____savegprlr_22(ctx, base); }
PPC_FUNC(__restgprlr_22) { __imp____restgprlr_22(ctx, base); }
"""
    originals = b"\n".join((prelude,
        abi_helper(args.ppc_root, "savegprlr_22", 5509, False),
        abi_helper(args.ppc_root, "restgprlr_22", 6083, True),
        wrappers, manifest["entries"][0]["translated_body"].encode("utf-8")))
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/heap_block_resize_oracle.cpp")\
        .read_bytes()
    sources = ("heap_block_resize", "heap_segment", "heap", "memory_fill")
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to(Path.home() / "ownCloud"):
        raise ValueError("build outputs must stay outside the checkout/ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    fixture = output / "heap-block-resize.cpp"
    fixture.write_bytes(b'#include "ppc_context.h"\n#include <cstddef>\n' +
                        originals + b"\n" + harness)
    executable = output / "heap-block-resize.exe"
    receipt = output / "heap-block-resize-result.json"
    result = {"suite": "heap-block-resize", "status": "failed"}
    receipt.write_text(json.dumps(result) + "\n", encoding="utf-8")
    environment = compiler_environment()
    search_path = next(value for key, value in environment.items()
                       if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    if compiler is None:
        raise RuntimeError("native clang-cl unavailable")
    includes = [ROOT / "LostOdysseyRecompLib/ppc",
                ROOT / "tools/XenonRecomp/thirdparty/simde",
                ROOT / "LostOdysseyRecompSemantics/include"]
    command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MD",
               "/W4", "/WX", "-Wno-ignored-attributes", "-msse4.1",
               *["/I" + str(path) for path in includes], str(fixture),
               *[str(ROOT / "LostOdysseyRecompSemantics/src" / (x + ".cpp"))
                 for x in sources], "/Fo" + str(output) + "\\",
               "/Fe" + str(executable)]
    with (output / "heap-block-resize.log").open("w", encoding="utf-8") as log:
        for name, argv in (("compile", command), ("execute", [str(executable)])):
            started = time.perf_counter()
            completed = subprocess.run(argv, cwd=ROOT, env=environment,
                                       capture_output=True, text=True, timeout=300)
            result[name + "_seconds"] = round(time.perf_counter() - started, 3)
            output_text = completed.stdout + completed.stderr
            log.write(output_text)
            log.flush()
            print(output_text, end="", flush=True)
            if completed.returncode:
                raise subprocess.CalledProcessError(completed.returncode, argv)
            if name == "execute":
                result["summary"] = output_text.strip()
    result["status"] = "passed"
    result["compiler_flags"] = ["/W4", "/WX"]
    receipt.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result)


if __name__ == "__main__":
    main()
