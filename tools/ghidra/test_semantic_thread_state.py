"""Run native Windows original-PPC differential for thread failure-code leaves."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-thread-state-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
GENERATED = "LostOdysseyRecompLib/ppc/ppc_recomp.2.cpp"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "822CA188": "oracle_StoreThreadFailureCode",
    "822CA180": "oracle_ReportAllocationFailure",
}
EXPECTED_FUNCTION_SHA256 = {
    "822CA188": "2d424e6c58d8cde5fb1d75661c37e8c567fb162fbb0ce25a3a2b72e5efb4f8ce",
    "822CA180": "757a0f44e9ce568b9dd8a7772268c77edb5db2dfe2321d62be80425928a872a8",
}
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/thread_state.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/thread_state.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/thread_state_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    GENERATED,
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_thread_state.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, address: str, renamed: str) -> tuple[bytes, str]:
    symbol = f"sub_{address}".encode("ascii")
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one generated PPC body for {address}, got {len(bodies)}")
    original = bodies[0]
    return (original.replace(b"PPC_FUNC_IMPL(__imp__" + symbol + b")",
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
    log = output / "thread_state_oracle.log"
    log.write_text("", encoding="utf-8")
    paths = {addr: output / f"receipt-{addr}.json" for addr in FUNCTIONS}
    for path in paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    source_hashes = {name: sha((ROOT / name).read_bytes()) for name in SOURCE_FILES}
    functions = {addr: extract((ROOT / GENERATED).read_bytes(), addr, renamed)
                 for addr, renamed in FUNCTIONS.items()}
    for addr, (_, digest) in functions.items():
        if digest != EXPECTED_FUNCTION_SHA256[addr]:
            raise ValueError(f"generated PPC body changed at {addr}: {digest}")
    prelude = b'''#include "ppc_context.h"
PPC_EXTERN_FUNC(oracle_StoreThreadFailureCode);
PPC_EXTERN_FUNC(oracle_ReportAllocationFailure);
PPC_EXTERN_FUNC(sub_822CA188);
'''
    generated = b"\n".join([prelude, *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/thread_state_oracle.cpp").read_bytes()])
    fixture = output / "thread_state_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "thread_state_oracle.exe"
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
               str(fixture), str(ROOT / SOURCE_FILES[0]),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipts = {
        addr: {
            "status": "failed", "passed": False, "function_address": addr,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "generated_ppc_path": str(ROOT / GENERATED),
            "generated_ppc_sha256": source_hashes[GENERATED],
            "original_function": f"sub_{addr}",
            "original_function_sha256": functions[addr][1],
            "source_sha256": source_hashes,
            "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "boundaries": [
                "Generated PPC C++ oracle, not raw XEX or full thread-local runtime equivalence.",
                "822CA180 invokes the actual extracted 822CA188 body in the oracle.",
                "Only bounded ordinary guest memory; concurrent/MMIO effects are not modeled.",
                "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
            ],
        } for addr in FUNCTIONS
    }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        counts = {addr: int(count) for addr, count in re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(FUNCTIONS) or any(count < 10 for count in counts.values()):
            raise RuntimeError("both thread-state oracle completion lines with >=10 cases required")
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
