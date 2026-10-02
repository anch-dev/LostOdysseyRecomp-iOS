"""Native Windows original-PPC differential for 827CC428 heap growth."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-heap-growth-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "827CC428": ("LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp", "oracle_GrowHeap"),
    "823AE108": ("LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp", "oracle_CoalesceFreeBlocks"),
    "827CBA60": ("LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp", "oracle_InsertFreeBlocks"),
}
EXPECTED_FUNCTION_SHA256 = {
    "827CC428": "8f5ab5121338282af8368fd6f3db0b604e73223963caac8005193b9b842774a8",
    "823AE108": "969dade741098ec3c1db3688109c352659cce813b7d62b9a8e1307e4ea8c0389",
    "827CBA60": "e4df966d6628870c304f3eed2d338a962e7c2f7c7d0ac34632018426548cf060",
}
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
HELPERS = ("__savegprlr_27", "__restgprlr_27", "__savegprlr_25", "__restgprlr_25",
           "__savegprlr_28", "__restgprlr_28")
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/heap_growth.cpp",
    "LostOdysseyRecompSemantics/src/heap.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/heap_growth.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/heap.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/heap_growth_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    HELPER_SOURCE,
    *(item[0] for item in FUNCTIONS.values()),
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_heap_growth.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode("ascii") + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original generated PPC body for {symbol}, got {len(bodies)}")
    original = bodies[0]
    return (original.replace(b"PPC_FUNC_IMPL(__imp__" + symbol.encode("ascii") + b")",
                             b"PPC_FUNC(" + renamed.encode("ascii") + b")", 1), sha(original))


def publish(path: Path, data: dict) -> None:
    pending = path.with_suffix(".json.tmp")
    pending.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    pending.replace(path)


def logged(command: list[str], log: Path, environment: dict | None = None) -> str:
    with log.open("a", encoding="utf-8") as output:
        output.write("+ " + subprocess.list2cmdline(command) + "\n")
        output.flush()
        completed = subprocess.run(command, cwd=ROOT, env=environment, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output.write(completed.stdout)
        output.flush()
        print(completed.stdout, end="", flush=True)
        if completed.returncode:
            raise subprocess.CalledProcessError(completed.returncode, command)
        return completed.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT.resolve()) or output.is_relative_to((Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    log = output / "heap_growth_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt-827CC428.json"
    publish(receipt_path, {"status": "failed", "passed": False,
                           "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    sources = {name: sha((ROOT / name).read_bytes()) for name in dict.fromkeys(SOURCE_FILES)}
    helpers = [extract((ROOT / HELPER_SOURCE).read_bytes(), symbol, symbol) for symbol in HELPERS]
    functions = {addr: extract((ROOT / path).read_bytes(), f"sub_{addr}", renamed)
                 for addr, (path, renamed) in FUNCTIONS.items()}
    for addr, (_, digest) in functions.items():
        if digest != EXPECTED_FUNCTION_SHA256[addr]:
            raise ValueError(f"original generated PPC body changed at {addr}: {digest}")
    prelude = b'''#include "ppc_context.h"
PPC_EXTERN_FUNC(__savegprlr_27);
PPC_EXTERN_FUNC(__restgprlr_27);
PPC_EXTERN_FUNC(__savegprlr_25);
PPC_EXTERN_FUNC(__restgprlr_25);
PPC_EXTERN_FUNC(__savegprlr_28);
PPC_EXTERN_FUNC(__restgprlr_28);
PPC_EXTERN_FUNC(oracle_GrowHeap);
PPC_EXTERN_FUNC(oracle_CoalesceFreeBlocks);
PPC_EXTERN_FUNC(oracle_InsertFreeBlocks);
PPC_EXTERN_FUNC(sub_827CB778);
PPC_EXTERN_FUNC(sub_823AE108);
PPC_EXTERN_FUNC(sub_827CBA60);
PPC_EXTERN_FUNC(sub_827CC2C0);
PPC_EXTERN_FUNC(__imp__NtAllocateVirtualMemory);
PPC_EXTERN_FUNC(__imp__NtFreeVirtualMemory);
PPC_EXTERN_FUNC(__imp__RtlCompareMemoryUlong);
'''
    generated = b"\n".join([prelude, *(body for body, _ in helpers),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/heap_growth_oracle.cpp").read_bytes()])
    fixture = output / "heap_growth_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "heap_growth_oracle.exe"
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
               str(fixture), str(ROOT / SOURCE_FILES[0]), str(ROOT / SOURCE_FILES[1]),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipt = {
        "status": "failed", "passed": False, "function_address": "827CC428",
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "generated_ppc_path": str(ROOT / FUNCTIONS["827CC428"][0]),
        "generated_ppc_sha256": sources[FUNCTIONS["827CC428"][0]],
        "original_function": "sub_827CC428", "original_function_sha256": functions["827CC428"][1],
        "dependency_functions_sha256": {addr: value[1] for addr, value in functions.items() if addr != "827CC428"},
        "abi_helpers_sha256": dict(zip(HELPERS, (item[1] for item in helpers))),
        "source_sha256": sources,
        "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Native generated PPC C++ oracle, not raw XEX machine-code or full heap equivalence.",
            "Original Coalesce 823AE108 and Insert 827CBA60 compose in oracle; recovered versions compose in library.",
            "TryExpandSegment 827CB778, BuildSegment 827CC2C0 and kernel VM calls are synthetic callback boundaries.",
            "Bounded ordinary guest memory; concurrent/MMIO effects, arbitrary damaged links and game runtime are not modeled.",
            "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
            "ABI spill stack is excluded from byte equality; live frame locals, r1, LR and r27..r31 are checked.",
        ],
    }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        count = re.search(r"^PASS 827CC428 (\d+)$", result, re.M)
        if count is None or int(count.group(1)) < 10:
            raise RuntimeError("heap growth oracle completion line with >=10 cases required")
        receipt.update(status="passed", passed=True, cases=int(count.group(1)))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
