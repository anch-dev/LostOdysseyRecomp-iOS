"""Native Windows PPC differential for primary manager resize and its node helper."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-manager-resize-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "82295950": ("0", "oracle_ResizePrimaryManagerStorage",
                 "4d547a3d039aefcb459b2b72683746825a904f7505bb84e87f90045bfa5717bc"),
    "822A0738": ("0", "oracle_FindPrimaryResizeNode",
                 "83a7bc65a2c58ba52213da718624c7cbaa3c952ce542ae23183d6f5c4e9da201"),
    "82B7A0B0": ("175", "oracle_CopyGuestMemory",
                 "2b75cd7c5f78f0353174594e4acacff4d36ef7c9e99136d1980541668080b1b0"),
}
HELPERS = ("__savegprlr_26", "__restgprlr_26")
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
COMPILE_SOURCES = [
    "LostOdysseyRecompSemantics/src/manager_resize.cpp",
    "LostOdysseyRecompSemantics/src/memory_move.cpp",
]
SOURCE_FILES = [
    *COMPILE_SOURCES,
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_resize.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/memory_move.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/manager_resize_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp",
    HELPER_SOURCE, "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_manager_resize.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode() + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original PPC body for {symbol}, found {len(bodies)}")
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
        parser.error("output must be outside project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    log = output / "manager_resize_oracle.log"
    log.write_text("", encoding="utf-8")
    paths = {address: output / f"receipt-{address}.json" for address in ("82295950", "822A0738")}
    for path in paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    sources = {name: sha((ROOT / name).read_bytes()) for name in dict.fromkeys(SOURCE_FILES)}
    helpers = {name: extract((ROOT / HELPER_SOURCE).read_bytes(), name, name)
               for name in HELPERS}
    functions = {
        address: extract((ROOT / f"LostOdysseyRecompLib/ppc/ppc_recomp.{file}.cpp").read_bytes(),
                         f"sub_{address}", renamed)
        for address, (file, renamed, _) in FUNCTIONS.items()
    }
    for address, (_, _, expected) in FUNCTIONS.items():
        if functions[address][1] != expected:
            raise ValueError(f"original PPC body changed at {address}: {functions[address][1]}")
    prelude = [b'#include "ppc_context.h"',
               b'void oracle_Indirect(PPCContext&, std::uint8_t*, std::uint32_t);',
               b'#undef PPC_CALL_INDIRECT_FUNC',
               b'#define PPC_CALL_INDIRECT_FUNC(x) oracle_Indirect(ctx, base, x)']
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in HELPERS]
    prelude += [f"PPC_EXTERN_FUNC({renamed});".encode()
                for _, renamed, _ in FUNCTIONS.values()]
    prelude += [b'PPC_EXTERN_FUNC(sub_822A0738);',
                b'PPC_EXTERN_FUNC(sub_82B7A0B0);']
    generated = b"\n".join([b"\n".join(prelude),
                            *(body for body, _ in helpers.values()),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/manager_resize_oracle.cpp").read_bytes()])
    fixture = output / "manager_resize_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "manager_resize_oracle.exe"
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
    receipts = {}
    for address in paths:
        source = f"LostOdysseyRecompLib/ppc/ppc_recomp.{FUNCTIONS[address][0]}.cpp"
        receipts[address] = {
            "status": "failed", "passed": False, "function_address": address,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "generated_ppc_path": str(ROOT / source),
            "generated_ppc_sha256": sources[source],
            "original_function": f"sub_{address}",
            "original_function_sha256": functions[address][1],
            "dependency_functions_sha256": {
                key: body[1] for key, body in functions.items() if key != address},
            "abi_helpers_sha256": {name: body[1] for name, body in helpers.items()},
            "source_sha256": sources, "generated_fixture_path": str(fixture),
            "generated_fixture_sha256": sha(generated),
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "boundaries": [
                "Original generated PPC C++ bodies, not raw XEX machine-code equivalence.",
                "The 82295950 oracle composes original 822A0738 and 82B7A0B0 bodies.",
                "Manager vtable +4 allocation and +12 free remain synthetic; actual full PPC register arguments, callback snapshots and return are compared.",
                "Ordinary bounded guest memory only; no runtime, CRT or MMIO acceptance.",
                "Generic ABI saves/backchain are excluded from byte equality; forward-copy frame-8 spill is compared.",
                "Compiler/SIMDE installation recorded by path, not recursively hashed.",
            ],
        }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        counts = {address: int(count) for address, count in
                  re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(paths) or any(count < 12 for count in counts.values()):
            raise RuntimeError("both resize and node differential completion lines required")
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
