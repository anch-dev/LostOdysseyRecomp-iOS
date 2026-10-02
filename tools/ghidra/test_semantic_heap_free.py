"""Native Windows original-PPC differential for heap free and cleanup."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-heap-free-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "823ADE28": ("LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp", "oracle_FreeHeapBlock"),
    "823AE0BC": ("LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp", "oracle_LeaveHeapCriticalSection"),
    "823AE108": ("LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp", "oracle_CoalesceFreeBlocks"),
    "827CBA60": ("LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp", "oracle_InsertFreeBlocks"),
}
EXPECTED_FUNCTION_SHA256 = {
    "823ADE28": "68bfcc98d18c87f61d719e864851aba9b900ad8914c2559f9b85793141d2a247",
    "823AE0BC": "d091df6c37db36c7a6ff74634ded0143b700f536dec3b059badaff537130516b",
    "823AE108": "969dade741098ec3c1db3688109c352659cce813b7d62b9a8e1307e4ea8c0389",
    "827CBA60": "e4df966d6628870c304f3eed2d338a962e7c2f7c7d0ac34632018426548cf060",
}
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
HELPERS = ("__savegprlr_25", "__restgprlr_25", "__savegprlr_28", "__restgprlr_28")
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/heap_free.cpp",
    "LostOdysseyRecompSemantics/src/heap.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/heap_free.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/heap.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/heap_free_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    HELPER_SOURCE,
    *(item[0] for item in FUNCTIONS.values()),
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_heap_free.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode("ascii") + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original PPC body for {symbol}, got {len(bodies)}")
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
    log = output / "heap_free_oracle.log"
    log.write_text("", encoding="utf-8")
    paths = {addr: output / f"receipt-{addr}.json" for addr in ("823ADE28", "823AE0BC")}
    for path in paths.values():
        publish(path, {"status": "failed", "passed": False,
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
            raise ValueError(f"original function changed at {addr}: {digest}")
    prelude = b'''#include "ppc_context.h"
PPC_EXTERN_FUNC(__savegprlr_25);
PPC_EXTERN_FUNC(__restgprlr_25);
PPC_EXTERN_FUNC(__savegprlr_28);
PPC_EXTERN_FUNC(__restgprlr_28);
PPC_EXTERN_FUNC(oracle_FreeHeapBlock);
PPC_EXTERN_FUNC(oracle_LeaveHeapCriticalSection);
PPC_EXTERN_FUNC(oracle_CoalesceFreeBlocks);
PPC_EXTERN_FUNC(oracle_InsertFreeBlocks);
PPC_EXTERN_FUNC(sub_823AE108);
PPC_EXTERN_FUNC(sub_827CBA60);
PPC_EXTERN_FUNC(sub_823AE0BC);
PPC_EXTERN_FUNC(sub_827CC668);
PPC_EXTERN_FUNC(__imp__KeGetCurrentProcessType);
PPC_EXTERN_FUNC(__imp__KeBugCheckEx);
PPC_EXTERN_FUNC(__imp__RtlEnterCriticalSection);
PPC_EXTERN_FUNC(__imp__RtlLeaveCriticalSection);
PPC_EXTERN_FUNC(__imp__NtFreeVirtualMemory);
PPC_EXTERN_FUNC(__imp__RtlCompareMemoryUlong);
'''
    generated = b"\n".join([prelude, *(body for body, _ in helpers),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/heap_free_oracle.cpp").read_bytes()])
    fixture = output / "heap_free_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "heap_free_oracle.exe"
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
    receipts = {}
    for addr, (path, _) in FUNCTIONS.items():
        if addr not in paths:
            continue
        receipts[addr] = {
            "status": "failed", "passed": False, "function_address": addr,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "generated_ppc_path": str(ROOT / path), "generated_ppc_sha256": sources[path],
            "original_function": f"sub_{addr}", "original_function_sha256": functions[addr][1],
            "dependency_functions_sha256": {key: value[1] for key, value in functions.items() if key != addr},
            "abi_helpers_sha256": dict(zip(HELPERS, (item[1] for item in helpers))),
            "source_sha256": sources,
            "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "boundaries": [
                "Native generated PPC C++ oracle, not raw XEX machine-code or full heap equivalence.",
                "Coalesce 823AE108 and Insert 827CBA60 run original extracted PPC in oracle and recovered implementations in library.",
                "Kernel, decommit and debug comparison calls are synthetic callback boundaries.",
                "Exception entry 823AE094, native C++ unwinding, arbitrary damaged links, concurrent/MMIO effects and game runtime are not modeled.",
                "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
                "ABI spill stack is excluded from byte equality; live frame locals, r1, LR and r25..r31 are checked.",
            ],
        }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        counts = {addr: int(count) for addr, count in re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(paths) or any(count < 3 for count in counts.values()):
            raise RuntimeError("both heap free oracle completion lines are required")
        for addr, receipt in receipts.items():
            receipt.update(status="passed", passed=True, cases=counts[addr])
    except Exception as error:
        for receipt in receipts.values():
            receipt["reason"] = str(error)
        raise
    finally:
        for addr, path in paths.items():
            publish(path, receipts[addr])
    for path in paths.values():
        print(f"receipt: {path}")


if __name__ == "__main__":
    main()
