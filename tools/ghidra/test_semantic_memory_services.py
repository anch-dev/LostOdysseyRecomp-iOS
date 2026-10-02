"""Run five generated-PPC memory service wrapper oracles on native Windows.

Private generated source and sparse 4GiB test address spaces stay outside the
synced repository. Each function gets a separate, source-bound receipt.
"""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-memory-services-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "827C9E20": ("LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp", "oracle_AllocatePhysical",
                 "096efe02f69ea1a07c0e782847590ea8c840d5f8668242c5cd212566360e43b4"),
    "827C9EB8": ("LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp", "oracle_FreePhysical",
                 "4e0406151329ec0751d5722ed836d52f8d3596dd7da0ae8bd713f99570a5ee6f"),
    "827CAD38": ("LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp", "oracle_AllocateHeap",
                 "bfd617f0ec1e01c2220bafffad09dd02381b9247b603d8059f1ab1b609fec13b"),
    "827CAD80": ("LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp", "oracle_FreeHeap",
                 "c8e98b522ca07e4b8a824752fd34b888c6a2d2d1f843ad8509472e21213b0714"),
    "823ACC98": ("LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp", "sub_823ACC98",
                 "1bb41618d8427c8602b056049c6051c6f3c139dabcb054c58f195e37fe694afe"),
}
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/memory_services.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/memory_services.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/memory_services_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    "LostOdysseyRecomp/kernel/imports.cpp",
    "LostOdysseyRecomp/kernel/function.h",
    *(entry[0] for entry in FUNCTIONS.values()),
    "tools/tests/run.py",
    "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_memory_services.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode("ascii") + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original generated PPC body for {symbol}; found {len(bodies)}")
    original = bodies[0]
    renamed_body = original.replace(b"PPC_FUNC_IMPL(__imp__" + symbol.encode("ascii") + b")",
                                    b"PPC_FUNC(" + renamed.encode("ascii") + b")", 1)
    return renamed_body, sha(original)


def publish(path: Path, data: dict) -> None:
    pending = path.with_suffix(".json.tmp")
    pending.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    pending.replace(path)


def run_logged(command: list[str], log: Path, environment: dict | None = None) -> str:
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
    log = output / "memory_services_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_paths = {address: output / f"receipt-{address}.json" for address in FUNCTIONS}
    for path in receipt_paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}; expected {EXPECTED_XEX_SHA256}")
    source_hashes = {path: sha((ROOT / path).read_bytes()) for path in dict.fromkeys(SOURCE_FILES)}
    extracted = {}
    for address, (path, renamed, expected_hash) in FUNCTIONS.items():
        extracted[address] = extract((ROOT / path).read_bytes(), f"sub_{address}", renamed)
        if extracted[address][1] != expected_hash:
            raise ValueError(f"generated PPC body changed at {address}: {extracted[address][1]}")

    prelude = b'''#include "ppc_context.h"
PPC_EXTERN_FUNC(__imp__MmAllocatePhysicalMemoryEx);
PPC_EXTERN_FUNC(__imp__MmFreePhysicalMemory);
PPC_EXTERN_FUNC(sub_822CA180);
PPC_EXTERN_FUNC(sub_823ACC98);
PPC_EXTERN_FUNC(sub_823ACCB0);
PPC_EXTERN_FUNC(sub_823ADE28);
PPC_EXTERN_FUNC(oracle_AllocatePhysical);
PPC_EXTERN_FUNC(oracle_FreePhysical);
PPC_EXTERN_FUNC(oracle_AllocateHeap);
PPC_EXTERN_FUNC(oracle_FreeHeap);
'''
    generated = b"\n".join([prelude, *(body for body, _ in extracted.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/memory_services_oracle.cpp").read_bytes()])
    fixture = output / "memory_services_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "memory_services_oracle.exe"
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
               str(fixture), str(ROOT / "LostOdysseyRecompSemantics/src/memory_services.cpp"),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipts = {}
    for address, (path, _, _) in FUNCTIONS.items():
        receipts[address] = {
            "status": "failed", "passed": False, "function_address": address,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "generated_ppc_path": str(ROOT / path),
            "generated_ppc_sha256": source_hashes[path],
            "original_function": f"sub_{address}",
            "original_function_sha256": extracted[address][1],
            "source_sha256": source_hashes,
            "void_import_evidence": {
                "native_import": "LostOdysseyRecomp/kernel/imports.cpp:793",
                "host_bridge": "LostOdysseyRecomp/kernel/function.h:239-242",
            },
            "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "boundaries": [
                "Native generated PPC C++ oracle, not raw XEX or game-runtime equivalence.",
                "Kernel imports, guest error routine and heap operations use synthetic callbacks.",
                "4GiB guest address space is reserved, but only low and required high pages are committed and compared.",
                "Void MmFreePhysicalMemory residual r3 is opaque and excluded from return comparison.",
                "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
            ],
        }
    try:
        run_logged(command, log, environment)
        result = run_logged([str(executable)], log)
        counts = {address: int(count) for address, count in
                  re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(FUNCTIONS) or any(count < 32 for count in counts.values()):
            raise RuntimeError("five oracle completion lines with >=32 cases each are required")
        for address, receipt in receipts.items():
            receipt.update(status="passed", passed=True, cases=counts[address])
    except Exception as error:
        for receipt in receipts.values():
            receipt["reason"] = str(error)
        raise
    finally:
        for address, path in receipt_paths.items():
            publish(path, receipts[address])
    for path in receipt_paths.values():
        print(f"receipt: {path}")


if __name__ == "__main__":
    main()
