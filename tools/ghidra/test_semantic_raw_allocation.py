"""Native Windows original-PPC differential for the raw allocation wrapper."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "tests"))
from run import compiler_environment  # noqa: E402

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-raw-allocation-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "823ACBD0": "oracle_AllocateRawMemory",
    "823ACC98": "oracle_GetProcessHeap",
}
EXPECTED_FUNCTION_SHA256 = {
    "823ACBD0": "07d28ba11b44a29a61d0cc9fb912ba33e3aadc42745fb47d09c6a95e65c7eb92",
    "823ACC98": "1bb41618d8427c8602b056049c6051c6f3c139dabcb054c58f195e37fe694afe",
}
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
HELPERS = ("__savegprlr_28", "__restgprlr_28")
COMPILE_SOURCES = [
    "LostOdysseyRecompSemantics/src/raw_allocation.cpp",
    "LostOdysseyRecompSemantics/src/memory_services.cpp",
]
SOURCE_FILES = [
    *COMPILE_SOURCES,
    "LostOdysseyRecompSemantics/include/lo_semantics/raw_allocation.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/memory_services.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/raw_allocation_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    "LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp",
    HELPER_SOURCE,
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_raw_allocation.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode() + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original PPC body for {symbol}, found {len(bodies)}")
    original = bodies[0]
    return (original.replace(b"PPC_FUNC_IMPL(__imp__" + symbol.encode() + b")",
                             b"PPC_FUNC(" + renamed.encode() + b")", 1), sha(original))


def publish(path: Path, data: dict) -> None:
    pending = path.with_suffix(".json.tmp")
    pending.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    pending.replace(path)


def logged(command: list[str], log: Path, environment: dict | None = None) -> str:
    with log.open("a", encoding="utf-8") as output:
        output.write("+ " + subprocess.list2cmdline(command) + "\n")
        output.flush()
        completed = subprocess.run(command, cwd=ROOT, env=environment, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   timeout=60)
        output.write(completed.stdout)
        output.flush()
        print(completed.stdout, end="", flush=True)
        if completed.returncode:
            raise subprocess.CalledProcessError(completed.returncode, command)
        return completed.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    output = parser.parse_args().output.resolve()
    if output.is_relative_to(ROOT.resolve()) or output.is_relative_to((Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    log = output / "raw_allocation_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt-823ACBD0.json"
    publish(receipt_path, {"status": "failed", "passed": False,
                           "reason": "raw allocation run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    sources = {name: sha((ROOT / name).read_bytes()) for name in dict.fromkeys(SOURCE_FILES)}
    helpers = {name: extract((ROOT / HELPER_SOURCE).read_bytes(), name, name)
               for name in HELPERS}
    functions = {addr: extract((ROOT / "LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp").read_bytes(),
                               f"sub_{addr}", renamed)
                 for addr, renamed in FUNCTIONS.items()}
    for addr, (_, digest) in functions.items():
        if digest != EXPECTED_FUNCTION_SHA256[addr]:
            raise ValueError(f"original PPC body changed at {addr}: {digest}")
    prelude = [b'#include "ppc_context.h"']
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in HELPERS]
    prelude += [f"PPC_EXTERN_FUNC({renamed});".encode() for renamed in FUNCTIONS.values()]
    prelude += [b'PPC_EXTERN_FUNC(sub_823ACC98);']
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in
                ("sub_823ACCB0", "sub_82B7FCE0", "sub_82B7FC98",
                 "sub_82B7BF20", "sub_82B7FE68", "sub_82B7FD78")]
    generated = b"\n".join([b"\n".join(prelude),
                            *(body for body, _ in helpers.values()),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/raw_allocation_oracle.cpp").read_bytes()])
    fixture = output / "raw_allocation_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "raw_allocation_oracle.exe"
    environment = compiler_environment()
    search_path = next(value for key, value in environment.items() if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    if compiler is None:
        raise RuntimeError("clang-cl missing after native Windows compiler setup")
    command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/Od", "/MD",
               "-Wno-ignored-attributes", "-msse4.1",
               "/I" + str(ROOT / "LostOdysseyRecompLib/ppc"),
               "/I" + str(ROOT / "tools/XenonRecomp/thirdparty/simde"),
               "/I" + str(ROOT / "LostOdysseyRecompSemantics/include"),
               str(fixture), *(str(ROOT / name) for name in COMPILE_SOURCES),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipt = {
        "status": "failed", "passed": False, "function_address": "823ACBD0",
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "generated_ppc_path": str(ROOT / "LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp"),
        "generated_ppc_sha256": sources["LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp"],
        "original_function": "sub_823ACBD0",
        "original_function_sha256": functions["823ACBD0"][1],
        "dependency_functions_sha256": {"823ACC98": functions["823ACC98"][1]},
        "abi_helpers_sha256": {name: body[1] for name, body in helpers.items()},
        "source_sha256": sources,
        "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Original generated PPC C++ bodies, not raw XEX machine-code equivalence.",
            "Original 823ACC98 process heap lookup is composed, including original 0x83245708 guest global; retry flag uses original 0x832d3aec guest global.",
            "Heap allocation, CRT missing-heap/error/retry callbacks are synthetic; actual PPC register arguments and callback order are compared.",
            "Sparse 4-GiB guest address space commits low memory and two original global pages only; no runtime/MMIO/concurrency.",
            "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
            "ABI spill stack is excluded from byte equality; r1, LR, r28..r31 and full 64-bit return are checked.",
        ],
    }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        matches = re.findall(r"^PASS 823ACBD0 (\d+)$", result, re.M)
        if len(matches) != 1 or int(matches[0]) < 20:
            raise RuntimeError("raw allocation differential completion line required")
        receipt.update(status="passed", passed=True, cases=int(matches[0]))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
