"""Native Windows original-PPC differential for three manager lock functions."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-manager-lock-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
FUNCTIONS = {
    "829664E8": ("LostOdysseyRecompLib/ppc/ppc_recomp.124.cpp",
                 "oracle_ReturnZeroStatus",
                 "f0730ff292592286ae4e06186694a154ef12f8e5b41c2fba7962880df0dce984"),
    "822958F8": ("LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp",
                 "oracle_StoreLockAndWaitForEnter",
                 "b4057a26d9fed0a651de1e70938e5aa49d8b9250b36b9a0492ef3d08acd04c4b"),
    "827C5688": ("LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp",
                 "oracle_InvokeManagerUnderLock",
                 "aeaa8a1553e0cfbdaeea141122c2570c7e59ef6f9e5c000bc4134a9d5adf41f6"),
}
HELPERS = {
    "__savegprlr_29": "2358ed1a9f8d6dd520b25318b93e2405baaa8d5e8126200fdd7c9b595182cd5f",
    "__restgprlr_29": "975666345b5ecf3ef1187776ad7dd5d1138adcfb34093dabc104620728387b3f",
}
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/manager_lock.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_lock.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/manager_lock_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    HELPER_SOURCE,
    *(item[0] for item in FUNCTIONS.values()),
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_manager_lock.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode() + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original generated PPC body for {symbol}, got {len(bodies)}")
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
    log = output / "manager_lock_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_paths = {address: output / f"receipt-{address}.json" for address in FUNCTIONS}
    receipt_paths["composition"] = output / "receipt-manager-lock-composed.json"
    for path in receipt_paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    sources = {name: sha((ROOT / name).read_bytes())
               for name in dict.fromkeys(SOURCE_FILES)}
    functions = {address: extract((ROOT / source).read_bytes(),
                                  f"sub_{address}", renamed)
                 for address, (source, renamed, _) in FUNCTIONS.items()}
    helpers = {name: extract((ROOT / HELPER_SOURCE).read_bytes(), name, name)
               for name in HELPERS}
    for address, (_, _, expected) in FUNCTIONS.items():
        if functions[address][1] != expected:
            raise ValueError(f"original generated PPC body changed at {address}: {functions[address][1]}")
    for name, expected in HELPERS.items():
        if helpers[name][1] != expected:
            raise ValueError(f"original ABI helper changed at {name}: {helpers[name][1]}")
    prelude = [
        b'#define PPC_CALL_INDIRECT_FUNC(address) oracle_Indirect(ctx, base, (address))',
        b'#include "ppc_context.h"',
        b'void oracle_Indirect(PPCContext& ctx, uint8_t* base, uint32_t method);',
    ]
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in HELPERS]
    prelude += [f"PPC_EXTERN_FUNC({renamed});".encode()
                for _, renamed, _ in FUNCTIONS.values()]
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in (
        "sub_822958F8", "__imp__RtlTryEnterCriticalSection",
        "__imp__RtlLeaveCriticalSection")]
    generated = b"\n".join([b"\n".join(prelude),
                            *(body for body, _ in helpers.values()),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/manager_lock_oracle.cpp").read_bytes()])
    fixture = output / "manager_lock_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "manager_lock_oracle.exe"
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
    common = {
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "original_functions_sha256": {address: body[1]
                                      for address, body in functions.items()},
        "abi_helpers_sha256": {name: body[1]
                               for name, body in helpers.items()},
        "source_sha256": sources,
        "generated_fixture_path": str(fixture),
        "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Native original-generated-PPC C++ differential, not raw XEX machine code or game runtime equivalence.",
            "822958F8 is actually called inside the original 827C5688 body and the recovered wrapper calls its recovered implementation.",
            "The zero-return 829664E8 body is called as an indirect method in composed cases; Try/Leave and nonzero virtual methods are synthetic callbacks.",
            "Full committed ordinary guest bytes, full r3 return, callback arguments/order and before/after memory fingerprints are compared.",
            "ABI saves and backchains are explicitly replayed by a test-only semantic adapter; production code does not implement a complete PPCContext adapter.",
            "No fault, MMIO, concurrency, volatile-context, or game-runtime equivalence.",
            "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
        ],
    }
    receipts = {}
    for address, (source, _, _) in FUNCTIONS.items():
        receipts[address] = dict(
            common, status="failed", passed=False,
            function_address=address,
            original_function=f"sub_{address}",
            original_function_sha256=functions[address][1],
            generated_ppc_path=str(ROOT / source),
            generated_ppc_sha256=sources[source],
        )
    receipts["composition"] = dict(
        common, status="failed", passed=False,
        suite="manager-lock-composed",
        composition="827C5688+822958F8+829664E8",
    )
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        expected = {"829664E8": 16, "822958F8": 24,
                    "827C5688": 24, "composition": 12}
        for key, minimum in expected.items():
            label = "manager-lock-composed" if key == "composition" else key
            match = re.search(rf"^PASS {label} (\d+)$", result, re.M)
            if match is None or int(match.group(1)) < minimum:
                raise RuntimeError(f"completion line for {label} needs >={minimum} cases")
            receipts[key].update(status="passed", passed=True,
                                 cases=int(match.group(1)))
    except Exception as error:
        for receipt in receipts.values():
            receipt["reason"] = str(error)
        raise
    finally:
        for key, path in receipt_paths.items():
            publish(path, receipts[key])
    for path in receipt_paths.values():
        print(f"receipt: {path}")


if __name__ == "__main__":
    main()
