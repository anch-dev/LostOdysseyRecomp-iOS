"""Native Windows original-PPC differential for heap range nodes and records."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-heap-ranges-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "827CB498": ("LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp", "oracle_AllocateRangeNode"),
    "827CB658": ("LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp", "oracle_InsertRangeRecord"),
}
EXPECTED_FUNCTION_SHA256 = {
    "827CB498": "a8a9018af697940f1532ac1b3826313d6cf3d8d06d0c996de539cdebdd84986c",
    "827CB658": "c862ff255d345d90bcd94ff36ad1cf6007e214e527bc08eb6463e972cdca02eb",
}
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
HELPERS = ("__savegprlr_28", "__restgprlr_28")
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/heap_ranges.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/heap_ranges.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/heap_ranges_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    HELPER_SOURCE,
    *(item[0] for item in FUNCTIONS.values()),
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_heap_ranges.py",
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
    log = output / "heap_ranges_oracle.log"
    log.write_text("", encoding="utf-8")
    paths = {addr: output / f"receipt-{addr}.json" for addr in ("827CB498", "827CB658")}
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
            raise ValueError(f"original generated PPC body changed at {addr}: {digest}")
    prelude = b'''#include "ppc_context.h"
PPC_EXTERN_FUNC(__savegprlr_28);
PPC_EXTERN_FUNC(__restgprlr_28);
PPC_EXTERN_FUNC(oracle_AllocateRangeNode);
PPC_EXTERN_FUNC(oracle_InsertRangeRecord);
PPC_EXTERN_FUNC(sub_827CB498);
PPC_EXTERN_FUNC(__imp__NtAllocateVirtualMemory);
PPC_EXTERN_FUNC(__imp__NtFreeVirtualMemory);
'''
    generated = b"\n".join([prelude, *(body for body, _ in helpers),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/heap_ranges_oracle.cpp").read_bytes()])
    fixture = output / "heap_ranges_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "heap_ranges_oracle.exe"
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
               str(fixture), *(str(ROOT / name) for name in SOURCE_FILES[:1]),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipts = {}
    for addr in paths:
        path, _ = FUNCTIONS[addr]
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
                "Original AllocateRangeNode 827CB498 composes inside InsertRangeRecord 827CB658; the recovered counterparts compose likewise.",
                "NtAllocateVirtualMemory and NtFreeVirtualMemory remain synthetic callbacks; no real VM or exception handling is exercised.",
                "Bounded ordinary guest heap; arbitrary damaged links, concurrent/MMIO effects and game runtime are not modeled.",
                "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
                "ABI spill stack is excluded from byte equality; live frame locals, r1, LR and r28..r31 are checked.",
            ],
        }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        counts = {addr: int(count) for addr, count in re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(paths) or any(count < 20 for count in counts.values()):
            raise RuntimeError("heap range nodes and records oracle completion line is required")
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
