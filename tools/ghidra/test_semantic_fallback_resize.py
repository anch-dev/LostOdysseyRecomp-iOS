"""Native Windows original-PPC differential for fallback resize and vector append."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-fallback-resize-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
PPC0 = "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp"
PPC75 = "LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp"
PPC175 = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
FUNCTIONS = {
    "82295530": (PPC0, "oracle_ResizeFallbackAllocation",
                 "527a7968f9974e6f14a5f3a0f0b25c4cee45326182730986a774c652fbd0d294"),
    "827C5050": (PPC75, "oracle_AppendPointerToVector",
                 "85de71f1c51810b81ac91451f532ac8662e9c782c5ab1f05efeb841dba6d5688"),
}
DEPENDENCIES = {
    "sub_822958F8": (PPC0, "oracle_StoreLockAndWaitForEnter",
                         "b4057a26d9fed0a651de1e70938e5aa49d8b9250b36b9a0492ef3d08acd04c4b"),
    "sub_82B7A0B0": (PPC175, "sub_82B7A0B0",
                         "2b75cd7c5f78f0353174594e4acacff4d36ef7c9e99136d1980541668080b1b0"),
}
HELPERS = {
    "__savegprlr_23": "c34117dcfabf17c4c216c7c54b2fc36682b685b0b96ea46c2f6edf6ab2d593fa",
    "__restgprlr_23": "b07f72b2bd38147606d4c3bfc92d0d841b9aa4d166ec2d63c35cdbd3e78fdc02",
    "__savegprlr_29": "2358ed1a9f8d6dd520b25318b93e2405baaa8d5e8126200fdd7c9b595182cd5f",
    "__restgprlr_29": "975666345b5ecf3ef1187776ad7dd5d1138adcfb34093dabc104620728387b3f",
}
COMPILE_SOURCES = [
    "LostOdysseyRecompSemantics/src/fallback_resize.cpp",
    "LostOdysseyRecompSemantics/src/manager_lock.cpp",
    "LostOdysseyRecompSemantics/src/memory_move.cpp",
]
SOURCE_FILES = [
    *COMPILE_SOURCES,
    "LostOdysseyRecompSemantics/include/lo_semantics/fallback_resize.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_lock.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/memory_move.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/fallback_resize_oracle.cpp",
    PPC0, PPC75, PPC175,
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_fallback_resize.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode() + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original PPC body for {symbol}, got {len(bodies)}")
    original = bodies[0]
    generated = original.replace(
        b"PPC_FUNC_IMPL(__imp__" + symbol.encode() + b")",
        b"PPC_FUNC(" + renamed.encode() + b")", 1)
    return generated, sha(original)


def publish(path: Path, data: dict) -> None:
    pending = path.with_suffix(".json.tmp")
    pending.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    pending.replace(path)


def logged(command: list[str], log: Path, environment: dict | None = None) -> str:
    with log.open("a", encoding="utf-8") as output:
        output.write("+ " + subprocess.list2cmdline(command) + "\n")
        output.flush()
        result = subprocess.run(command, cwd=ROOT, env=environment,
                                text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=180)
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
    if output.is_relative_to(ROOT.resolve()) or output.is_relative_to(
            (Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    log = output / "fallback_resize_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_paths = {address: output / f"receipt-{address}.json"
                     for address in FUNCTIONS}
    receipt_paths["composition"] = output / "receipt-fallback-resize-composed.json"
    for path in receipt_paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed",
                       "log_path": str(log)})
    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    sources = {name: sha((ROOT / name).read_bytes())
               for name in dict.fromkeys(SOURCE_FILES)}
    functions = {address: extract((ROOT / source).read_bytes(),
                                  f"sub_{address}", renamed)
                 for address, (source, renamed, _) in FUNCTIONS.items()}
    dependencies = {name: extract((ROOT / source).read_bytes(), name, renamed)
                    for name, (source, renamed, _) in DEPENDENCIES.items()}
    helpers = {name: extract((ROOT / PPC175).read_bytes(), name, name)
               for name in HELPERS}
    for address, (_, _, expected) in FUNCTIONS.items():
        if functions[address][1] != expected:
            raise ValueError(f"original body changed at {address}: {functions[address][1]}")
    for name, (_, _, expected) in DEPENDENCIES.items():
        if dependencies[name][1] != expected:
            raise ValueError(f"dependency body changed at {name}: {dependencies[name][1]}")
    for name, expected in HELPERS.items():
        if helpers[name][1] != expected:
            raise ValueError(f"ABI helper changed at {name}: {helpers[name][1]}")
    externs = [
        *HELPERS, "oracle_ResizeFallbackAllocation",
        "oracle_AppendPointerToVector", "oracle_StoreLockAndWaitForEnter",
        "sub_827C5050", "sub_822958F8", "sub_82B7A0B0",
        "sub_827C4FA0", "__imp__RtlTryEnterCriticalSection",
        "__imp__RtlLeaveCriticalSection",
    ]
    prelude = [
        b'#define PPC_CALL_INDIRECT_FUNC(address) oracle_Indirect(ctx, base, (address))',
        b'#include "ppc_context.h"',
        b'void oracle_Indirect(PPCContext& ctx, uint8_t* base, uint32_t method);',
        *(f"PPC_EXTERN_FUNC({name});".encode() for name in externs),
    ]
    generated = b"\n".join([
        b"\n".join(prelude),
        *(body for body, _ in helpers.values()),
        *(body for body, _ in dependencies.values()),
        *(body for body, _ in functions.values()),
        (ROOT / "LostOdysseyRecompSemantics/tests/fallback_resize_oracle.cpp").read_bytes(),
    ])
    fixture = output / "fallback_resize_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "fallback_resize_oracle.exe"
    environment = compiler_environment()
    search_path = next(value for key, value in environment.items()
                       if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    if compiler is None:
        raise RuntimeError("clang-cl missing after native Windows compiler setup")
    command = [
        compiler, "/nologo", "/std:c++20", "/EHsc", "/Od", "/MD",
        "-Wno-ignored-attributes", "-msse4.1",
        "/I" + str(ROOT / "LostOdysseyRecompLib/ppc"),
        "/I" + str(ROOT / "tools/XenonRecomp/thirdparty/simde"),
        "/I" + str(ROOT / "LostOdysseyRecompSemantics/include"),
        str(fixture), *(str(ROOT / name) for name in COMPILE_SOURCES),
        "/Fo" + str(output) + "\\", "/Fe" + str(executable),
    ]
    common = {
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "original_functions_sha256": {
            address: item[1] for address, item in functions.items()},
        "dependency_functions_sha256": {
            name: item[1] for name, item in dependencies.items()},
        "abi_helpers_sha256": {name: item[1] for name, item in helpers.items()},
        "source_sha256": sources,
        "generated_fixture_path": str(fixture),
        "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Native original-generated-PPC C++ differential, not raw XEX machine code or runtime equivalence.",
            "Original 82295530 calls actual original 827C5050, 822958F8 and 82B7A0B0 bodies; recovered implementation composes their recovered counterparts.",
            "827C4FA0 growth, manager virtual +8/+12 and kernel Try/Leave remain synthetic callback boundaries.",
            "Compares full r3, restored original nonvolatile registers, ordinary guest memory, frame+80, and callback argument/order/before-after ordinary memory.",
            "ABI save/backchain guest writes are explicit test-only adapter writes; generic stack scratch is excluded from ordinary comparison.",
            "No fault, MMIO, concurrency, volatile-context, or game-runtime equivalence.",
            "SIMDE include tree and compiler installation recorded by path, not fully hashed.",
        ],
    }
    receipts = {}
    for address, (source, _, _) in FUNCTIONS.items():
        receipts[address] = dict(
            common, status="failed", passed=False, function_address=address,
            original_function=f"sub_{address}",
            original_function_sha256=functions[address][1],
            generated_ppc_path=str(ROOT / source),
            generated_ppc_sha256=sources[source],
        )
    receipts["composition"] = dict(
        common, status="failed", passed=False,
        suite="fallback-resize-composed",
        composition="82295530+827C5050+822958F8+82B7A0B0",
    )
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        expected = {"827C5050": 4, "82295530": 39, "composition": 12}
        for key, minimum in expected.items():
            label = "fallback-resize-composed" if key == "composition" else key
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
