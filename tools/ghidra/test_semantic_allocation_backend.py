"""Differentially test two allocation backend functions against original PPC.

The original generated source is extracted to a native Windows output folder
outside the synced repository; it is never copied into tracked files.
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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-allocation-backend-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
GENERATED = "LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp"
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
FUNCTIONS = {"827CA050": "oracle_AllocateGeneral", "827CA0E8": "oracle_FreeGeneral"}
EXPECTED_FUNCTION_SHA256 = {
    "827CA050": "b0dec64d657701f923afdcf7bdd7dd94d2da2be9e6a8a5b1eb47269e43297873",
    "827CA0E8": "5fdc0a4f64b70281fa97d3bfe2dc99afba58c972c4e719b7882a21aff579779b",
}
HELPERS = ("__savegprlr_29", "__restgprlr_29")
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/allocation_backend.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/allocation_backend.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/allocation_backend_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    GENERATED, HELPER_SOURCE,
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_allocation_backend.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode("ascii") + rb"\) \{.*?\r?\n\}"
    matches = re.findall(pattern, source, re.S)
    if len(matches) != 1:
        raise ValueError(f"expected one original generated function {symbol}, got {len(matches)}")
    original = matches[0]
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
    log = output / "allocation_backend_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_paths = {addr: output / f"receipt-{addr}.json" for addr in FUNCTIONS}
    for path in receipt_paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}; expected {EXPECTED_XEX_SHA256}")
    source_hashes = {name: sha((ROOT / name).read_bytes()) for name in SOURCE_FILES}
    original = (ROOT / GENERATED).read_bytes()
    helpers = (ROOT / HELPER_SOURCE).read_bytes()
    function_bodies = {addr: extract(original, f"sub_{addr}", renamed)
                       for addr, renamed in FUNCTIONS.items()}
    for addr, (_, digest) in function_bodies.items():
        if digest != EXPECTED_FUNCTION_SHA256[addr]:
            raise ValueError(f"generated PPC body changed at {addr}: {digest}")
    helper_bodies = [extract(helpers, name, name) for name in HELPERS]
    prelude = b'''#include "ppc_context.h"
PPC_EXTERN_FUNC(__savegprlr_29);
PPC_EXTERN_FUNC(__restgprlr_29);
PPC_EXTERN_FUNC(sub_827C9E20);
PPC_EXTERN_FUNC(sub_827CAD38);
PPC_EXTERN_FUNC(sub_82B7BC40);
PPC_EXTERN_FUNC(sub_827C9EB8);
PPC_EXTERN_FUNC(sub_827CAD80);
PPC_EXTERN_FUNC(oracle_AllocateGeneral);
PPC_EXTERN_FUNC(oracle_FreeGeneral);
'''
    generated = b"\n".join([prelude, *(body for body, _ in helper_bodies),
                            *(body for body, _ in function_bodies.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/allocation_backend_oracle.cpp").read_bytes()])
    fixture = output / "allocation_backend_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "allocation_backend_oracle.exe"
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
    receipts = {}
    for addr in FUNCTIONS:
        receipts[addr] = {
            "status": "failed", "passed": False, "function_address": addr,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "generated_ppc_path": str(ROOT / GENERATED),
            "generated_ppc_sha256": source_hashes[GENERATED],
            "original_function": f"sub_{addr}",
            "original_function_sha256": function_bodies[addr][1],
            "abi_helpers_sha256": dict(zip(HELPERS, (item[1] for item in helper_bodies))),
            "source_sha256": source_hashes,
            "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "boundaries": [
                "Native generated PPC C++ oracle, not raw XEX or gameplay equivalence.",
                "The original guest protection table page at 0x831E7000 is committed; other high guest pages are not.",
                "Heap, physical and fill callbacks are synthetic, including their guest memory side effects.",
                "No concurrent mutation, MMIO or real allocator lifetime is modeled.",
                "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
                "Guest stack scratch 0x3E000..0x3FFFF is excluded from byte equality; r1, LR and r29..r31 are checked.",
            ],
        }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        counts = {addr: int(count) for addr, count in re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(FUNCTIONS) or any(count < 256 for count in counts.values()):
            raise RuntimeError("two oracle completion lines with >=256 cases each are required")
        for addr, receipt in receipts.items():
            receipt.update(status="passed", passed=True, cases=counts[addr])
    except Exception as error:
        for receipt in receipts.values():
            receipt["reason"] = str(error)
        raise
    finally:
        for addr, path in receipt_paths.items():
            publish(path, receipts[addr])
    for path in receipt_paths.values():
        print(f"receipt: {path}")


if __name__ == "__main__":
    main()
