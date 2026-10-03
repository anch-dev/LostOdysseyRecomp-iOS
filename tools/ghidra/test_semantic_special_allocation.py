"""Native Windows original-PPC differential for special block allocation/free."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-special-allocation-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "827C9A40": ("75", "oracle_AllocateSpecialBlock"),
    "827C9C60": ("75", "oracle_FreeSpecialBlock"),
    "82298AF8": ("0", "oracle_RemoveArrayRange"),
    "8229F678": ("0", "oracle_ResizeArray"),
    "82B7C470": ("175", "oracle_MoveGuestMemory"),
    "82B7A0B0": ("175", "oracle_CopyGuestMemory"),
}
EXPECTED_FUNCTION_SHA256 = {
    "827C9A40": "5ea0f7d13cc7add6bfea277fe69065fff61423c3ca926b393a9c85568636b64b",
    "827C9C60": "2919ccb767b1dc3c418e6f0dbaa681077f5c790d133282ecfe5677e7042d1be5",
    "82298AF8": "c687bd7e5033f3f1e41f689054814cbe7342caf70a381173c598d9c2521364d9",
    "8229F678": "6227cab1d3ba6e6bc1c07dbc66a9e66fa2ec4b7afc5b4dc5b80bc4e69ac22c04",
    "82B7C470": "ab4d8bb009f16a05550bf32b04b16850a7fe1cb834cc02fc7adedbdb9257118c",
    "82B7A0B0": "2b75cd7c5f78f0353174594e4acacff4d36ef7c9e99136d1980541668080b1b0",
}
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
HELPERS = tuple(f"__{kind}gprlr_{n}" for n in (22, 24, 27, 28)
                for kind in ("save", "rest"))
COMPILE_SOURCES = [
    "LostOdysseyRecompSemantics/src/special_allocation.cpp",
    "LostOdysseyRecompSemantics/src/allocation_array.cpp",
    "LostOdysseyRecompSemantics/src/memory_move.cpp",
]
SOURCE_FILES = [
    *COMPILE_SOURCES,
    "LostOdysseyRecompSemantics/include/lo_semantics/special_allocation.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/allocation_array.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/memory_move.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/special_allocation_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    *(f"LostOdysseyRecompLib/ppc/ppc_recomp.{file}.cpp" for file in ("75", "0", "175")),
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_special_allocation.py",
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
        completed = subprocess.run(command, cwd=ROOT, env=environment, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   timeout=60)
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
    log = output / "special_allocation_oracle.log"
    log.write_text("", encoding="utf-8")
    paths = {addr: output / f"receipt-{addr}.json" for addr in ("827C9A40", "827C9C60")}
    for path in paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "special allocation run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    sources = {name: sha((ROOT / name).read_bytes()) for name in dict.fromkeys(SOURCE_FILES)}
    helpers = {name: extract((ROOT / HELPER_SOURCE).read_bytes(), name, name)
               for name in HELPERS}
    functions = {addr: extract((ROOT / f"LostOdysseyRecompLib/ppc/ppc_recomp.{file}.cpp").read_bytes(),
                               f"sub_{addr}", renamed)
                 for addr, (file, renamed) in FUNCTIONS.items()}
    for addr, (_, digest) in functions.items():
        if digest != EXPECTED_FUNCTION_SHA256[addr]:
            raise ValueError(f"original PPC body changed at {addr}: {digest}")
    prelude = [b'#include "ppc_context.h"',
               b'void oracle_AllocationIndirect(PPCContext&, std::uint8_t*, std::uint32_t);',
               b'#undef PPC_CALL_INDIRECT_FUNC',
               b'#define PPC_CALL_INDIRECT_FUNC(x) oracle_AllocationIndirect(ctx, base, x)']
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in HELPERS]
    prelude += [f"PPC_EXTERN_FUNC({renamed});".encode()
                for _, renamed in FUNCTIONS.values()]
    prelude += [f"PPC_EXTERN_FUNC(sub_{addr});".encode() for addr in FUNCTIONS]
    prelude += [b'PPC_EXTERN_FUNC(sub_827C5F38);']
    generated = b"\n".join([b"\n".join(prelude),
                            *(body for body, _ in helpers.values()),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/special_allocation_oracle.cpp").read_bytes()])
    fixture = output / "special_allocation_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "special_allocation_oracle.exe"
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
    for addr in paths:
        file, _ = FUNCTIONS[addr]
        ppc_source = f"LostOdysseyRecompLib/ppc/ppc_recomp.{file}.cpp"
        receipts[addr] = {
            "status": "failed", "passed": False, "function_address": addr,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "generated_ppc_path": str(ROOT / ppc_source),
            "generated_ppc_sha256": sources[ppc_source],
            "original_function": f"sub_{addr}",
            "original_function_sha256": functions[addr][1],
            "dependency_functions_sha256": {key: body[1] for key, body in functions.items() if key != addr},
            "abi_helpers_sha256": {name: body[1] for name, body in helpers.items()},
            "source_sha256": sources,
            "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "boundaries": [
                "Original generated PPC C++ bodies, not raw XEX machine-code equivalence.",
                "Special allocation/free compose original array removal/resize, MoveGuestMemory and CopyGuestMemory bodies.",
                "Manager initialization and indirect allocator methods are synthetic; actual PPC register arguments are compared.",
                "FreeSpecialBlock's ordinary C++ API is void and does not expose original PPC r3 residue.",
                "Sparse 4-GiB guest address space commits low memory and the original global manager page only; no runtime/MMIO/concurrency.",
                "SIMDE include tree and native compiler installation are recorded by path, not fully hashed.",
                "Generic ABI spill stack is excluded from byte equality; CopyGuestMemory's nested frame-8 spill is compared in full unless ResizeArray subsequently saves LR over its high word.",
                "Allocation live frame locals, original r1, LR and nonvolatile GPRs are checked; FreeSpecialBlock has no exposed C++ r3 residue.",
            ],
        }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        counts = {addr: int(count) for addr, count in
                  re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(paths) or any(count < 5 for count in counts.values()):
            raise RuntimeError("special allocation differential completion lines required")
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
