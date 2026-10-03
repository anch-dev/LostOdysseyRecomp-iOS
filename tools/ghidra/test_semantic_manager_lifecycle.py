"""Native Windows differential for composed manager initialization dependencies."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-manager-lifecycle-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
PPC_RAW = "LostOdysseyRecompLib/ppc/ppc_recomp.11.cpp"
PPC_MANAGER = "LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp"
PPC_HELPERS = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
FUNCTIONS = {
    "827C5F38": (PPC_MANAGER, "oracle_InitializeManager",
                 "48e4b60654cdc9db936d96a4363ab85abc21753c32383593b62fbc24720e71a3"),
    "823ACBD0": (PPC_RAW, "sub_823ACBD0",
                 "07d28ba11b44a29a61d0cc9fb912ba33e3aadc42745fb47d09c6a95e65c7eb92"),
    "823ACC98": (PPC_RAW, "sub_823ACC98",
                 "1bb41618d8427c8602b056049c6051c6f3c139dabcb054c58f195e37fe694afe"),
    "827C5970": (PPC_MANAGER, "sub_827C5970",
                 "33746c1e9e40506ef8f5ce6d3fa251461a053c14df0320ad4d4bc349eb92582e"),
    "827C4ED0": (PPC_MANAGER, "sub_827C4ED0",
                 "e7ff4316cc46e90805b32daef108328a17e74dc1b67026285039992b79485b8f"),
}
HELPERS = ("__savegprlr_28", "__restgprlr_28")
COMPILE_SOURCES = [
    "LostOdysseyRecompSemantics/src/manager_init.cpp",
    "LostOdysseyRecompSemantics/src/raw_allocation.cpp",
    "LostOdysseyRecompSemantics/src/memory_services.cpp",
    "LostOdysseyRecompSemantics/src/manager_construction.cpp",
]
SOURCE_FILES = [
    *COMPILE_SOURCES,
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_init.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/raw_allocation.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/memory_services.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_construction.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/manager_lifecycle_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    PPC_RAW, PPC_MANAGER, PPC_HELPERS,
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_manager_lifecycle.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode() + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original generated PPC body for {symbol}, got {len(bodies)}")
    original = bodies[0]
    renamed_body = original.replace(
        b"PPC_FUNC_IMPL(__imp__" + symbol.encode() + b")",
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
        completed = subprocess.run(command, cwd=ROOT, env=environment,
                                   text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=120)
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
    log = output / "manager_lifecycle_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt-manager-lifecycle.json"
    publish(receipt_path, {"status": "failed", "passed": False,
                           "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    sources = {name: sha((ROOT / name).read_bytes()) for name in SOURCE_FILES}
    functions = {address: extract((ROOT / source).read_bytes(),
                                  f"sub_{address}", renamed)
                 for address, (source, renamed, _) in FUNCTIONS.items()}
    for address, (_, _, expected) in FUNCTIONS.items():
        if functions[address][1] != expected:
            raise ValueError(f"original generated PPC body changed at {address}: {functions[address][1]}")
    helpers = {name: extract((ROOT / PPC_HELPERS).read_bytes(), name, name)
               for name in HELPERS}
    prelude = [
        b'#define PPC_CALL_INDIRECT_FUNC(address) oracle_Indirect(ctx, base, (address))',
        b'#include "ppc_context.h"',
        b'void oracle_Indirect(PPCContext& ctx, uint8_t* base, uint32_t address);',
    ]
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in HELPERS]
    prelude += [f"PPC_EXTERN_FUNC({renamed});".encode()
                for _, renamed, _ in FUNCTIONS.values()]
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in (
        "sub_823ACCB0", "sub_82B7FCE0", "sub_82B7FC98",
        "sub_82B7BF20", "sub_82B7FE68", "sub_82B7FD78",
        "__imp__RtlInitializeCriticalSection")]
    generated = b"\n".join([b"\n".join(prelude),
                            *(body for body, _ in helpers.values()),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/manager_lifecycle_oracle.cpp").read_bytes()])
    fixture = output / "manager_lifecycle_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "manager_lifecycle_oracle.exe"
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
        "status": "failed", "passed": False,
        "composition": "827C5F38+823ACBD0+823ACC98+827C5970+827C4ED0",
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "original_functions_sha256": {address: result[1]
                                      for address, result in functions.items()},
        "abi_helpers_sha256": {name: result[1]
                               for name, result in helpers.items()},
        "source_sha256": sources,
        "generated_fixture_path": str(fixture),
        "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Native generated-PPC C++ composition, not raw XEX machine code or game runtime equivalence.",
            "Five original functions and two original ABI helpers compose against five recovered functions; a test-only adapter explicitly replays ABI frame stores/backchains, which the production semantic library does not automatically produce.",
            "Heap allocation, CRT retry/error calls, kernel critical section, and two virtual methods remain synthetic callbacks.",
            "Compares full committed ordinary guest memory, complete r3 return, ABI restoration, and callback arguments/order/before-after memory fingerprints.",
            "Covers primary/fallback/null allocation, first virtual callback global mutation, frame local alias with manager global, and CRT retry success.",
            "No fault, MMIO, concurrency, volatile-register, or game-runtime equivalence.",
            "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
        ],
    }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        count = re.search(r"^PASS manager-lifecycle (\d+)$", result, re.M)
        if count is None or int(count.group(1)) < 12:
            raise RuntimeError("manager lifecycle completion line with >=12 cases required")
        receipt.update(status="passed", passed=True, cases=int(count.group(1)))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
