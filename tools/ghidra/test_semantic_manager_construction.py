"""Native Windows PPC differential for the two allocator-manager constructors."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-manager-construction-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
PPC_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp"
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
FUNCTIONS = {
    "827C5970": ("oracle_ConstructPrimary", "33746c1e9e40506ef8f5ce6d3fa251461a053c14df0320ad4d4bc349eb92582e"),
    "827C4ED0": ("oracle_ConstructFallback", "e7ff4316cc46e90805b32daef108328a17e74dc1b67026285039992b79485b8f"),
}
HELPERS = ("__savegprlr_28", "__restgprlr_28")
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/manager_construction.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_construction.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/manager_construction_oracle.cpp",
    PPC_SOURCE, HELPER_SOURCE,
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_manager_construction.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode() + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one generated PPC body for {symbol}, found {len(bodies)}")
    original = bodies[0]
    renamed_body = original.replace(b"PPC_FUNC_IMPL(__imp__" + symbol.encode() + b")",
                                    b"PPC_FUNC(" + renamed.encode() + b")", 1)
    return renamed_body, sha(original)


def publish(path: Path, data: dict) -> None:
    pending = path.with_suffix(".json.tmp")
    pending.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    pending.replace(path)


def logged(command: list[str], log: Path, environment: dict | None = None) -> str:
    with log.open("a", encoding="utf-8") as output:
        output.write("+ " + subprocess.list2cmdline(command) + "\n")
        output.flush()
        result = subprocess.run(command, cwd=ROOT, env=environment, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=120)
        output.write(result.stdout)
        output.flush()
        print(result.stdout, end="", flush=True)
        if result.returncode:
            raise subprocess.CalledProcessError(result.returncode, command)
        return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    output = parser.parse_args().output.resolve()
    if output.is_relative_to(ROOT.resolve()) or output.is_relative_to((Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    log = output / "manager_construction_oracle.log"
    log.write_text("", encoding="utf-8")
    paths = {address: output / f"receipt-{address}.json" for address in FUNCTIONS}
    for path in paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    source_hashes = {name: sha((ROOT / name).read_bytes()) for name in SOURCE_FILES}
    ppc = (ROOT / PPC_SOURCE).read_bytes()
    functions = {address: extract(ppc, f"sub_{address}", renamed)
                 for address, (renamed, _) in FUNCTIONS.items()}
    for address, (_, expected) in FUNCTIONS.items():
        if functions[address][1] != expected:
            raise ValueError(f"original PPC body changed at {address}: {functions[address][1]}")
    helper_source = (ROOT / HELPER_SOURCE).read_bytes()
    helpers = {name: extract(helper_source, name, name) for name in HELPERS}
    prelude = [b'#include "ppc_context.h"',
               b'PPC_EXTERN_FUNC(__imp__RtlInitializeCriticalSection);']
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in HELPERS]
    prelude += [f"PPC_EXTERN_FUNC({renamed});".encode() for renamed, _ in FUNCTIONS.values()]
    generated = b"\n".join([b"\n".join(prelude),
                            *(body for body, _ in helpers.values()),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/manager_construction_oracle.cpp").read_bytes()])
    fixture = output / "manager_construction_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "manager_construction_oracle.exe"
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
               str(fixture), str(ROOT / "LostOdysseyRecompSemantics/src/manager_construction.cpp"),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipts = {}
    for address in paths:
        receipts[address] = {
            "status": "failed", "passed": False, "function_address": address,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "generated_ppc_path": str(ROOT / PPC_SOURCE),
            "generated_ppc_sha256": source_hashes[PPC_SOURCE],
            "original_function": f"sub_{address}",
            "original_function_sha256": functions[address][1],
            "abi_helpers_sha256": {name: body[1] for name, body in helpers.items()},
            "source_sha256": source_hashes,
            "generated_fixture_path": str(fixture),
            "generated_fixture_sha256": sha(generated),
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "boundaries": [
                "Compares generated PPC C++ bodies, not raw XEX instruction execution.",
                "Critical-section kernel import is a synthetic callback with actual r3 captured.",
                "Sparse 4-GiB guest space commits low memory and two high data pages only.",
                "Generic save/restore and backchain stack bytes are excluded for fallback; explicitly owned frame words are compared.",
                "Primary constant path compares the full mapped low window including its r1-16 spill.",
                "Compiler and SIMDE installation are recorded by path, not recursively hashed.",
            ],
        }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        counts = {address: int(count) for address, count in
                  re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(paths) or any(count < 20 for count in counts.values()):
            raise RuntimeError("both constructor differential completion lines required")
        for address, receipt in receipts.items():
            receipt.update(status="passed", passed=True, cases=counts[address])
    except Exception as error:
        for receipt in receipts.values():
            receipt["reason"] = str(error)
        raise
    finally:
        for address, path in paths.items():
            publish(path, receipts[address])
    for path in paths.values():
        print(f"receipt: {path}")


if __name__ == "__main__":
    main()
