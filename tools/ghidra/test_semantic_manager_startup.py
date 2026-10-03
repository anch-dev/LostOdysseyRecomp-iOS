"""Native Windows differential for the ten-function manager startup chain."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-manager-startup-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
DEFAULT_IMAGE = ROOT / "LostOdysseyRecompLib/private/image_disc1.bin"
EXPECTED_IMAGE_SHA256 = "cb756b46092e448923517bf660b44ad1a0651c2860882b43ae021f4ad4290f71"
IMAGE_BASE = 0x82000000
IMAGE_SIZE = 0x13C0000
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
FUNCTIONS = {
    "827C5F38": ("75", "oracle_InitializeManager", "48e4b60654cdc9db936d96a4363ab85abc21753c32383593b62fbc24720e71a3"),
    "823ACBD0": ("11", "sub_823ACBD0", "07d28ba11b44a29a61d0cc9fb912ba33e3aadc42745fb47d09c6a95e65c7eb92"),
    "823ACC98": ("11", "sub_823ACC98", "1bb41618d8427c8602b056049c6051c6f3c139dabcb054c58f195e37fe694afe"),
    "827C5970": ("75", "sub_827C5970", "33746c1e9e40506ef8f5ce6d3fa251461a053c14df0320ad4d4bc349eb92582e"),
    "827C4ED0": ("75", "sub_827C4ED0", "e7ff4316cc46e90805b32daef108328a17e74dc1b67026285039992b79485b8f"),
    "829664E8": ("124", "sub_829664E8", "f0730ff292592286ae4e06186694a154ef12f8e5b41c2fba7962880df0dce984"),
    "822958F8": ("0", "sub_822958F8", "b4057a26d9fed0a651de1e70938e5aa49d8b9250b36b9a0492ef3d08acd04c4b"),
    "827C5688": ("75", "sub_827C5688", "aeaa8a1553e0cfbdaeea141122c2570c7e59ef6f9e5c000bc4134a9d5adf41f6"),
    "827C5D88": ("75", "sub_827C5D88", "b82936f02593fd84101f2061b493e09a5eb5bd2fcd22917524626a88fde76388"),
    "827C5B30": ("75", "sub_827C5B30", "cc499bcb99d6af74de7d127c4d049b276a0d248ccf5b6765a041a5b09f17766a"),
}
HELPERS = {
    "__savegprlr_28": "12fbe4f938317fbf530c83470f4ff59198ed0f4ed212e1fa83e93026b4531d69",
    "__restgprlr_28": "a0064f80d05a0f3148b4efa625f3c0b0a39f1c8402ede58da8805d05955fadb0",
    "__savegprlr_29": "2358ed1a9f8d6dd520b25318b93e2405baaa8d5e8126200fdd7c9b595182cd5f",
    "__restgprlr_29": "975666345b5ecf3ef1187776ad7dd5d1138adcfb34093dabc104620728387b3f",
    "__savegprlr_26": "3d2cbfb013a4fa646448ad5d7802ca2abf1a3b7164249fd17cede65ffdf883ba",
    "__restgprlr_26": "fa77e55fdafc7a9935523ebf5cb76a36b3718e63efe8286862d15e53112cc6bb",
}
COMPILE_SOURCES = [
    "LostOdysseyRecompSemantics/src/manager_init.cpp",
    "LostOdysseyRecompSemantics/src/raw_allocation.cpp",
    "LostOdysseyRecompSemantics/src/memory_services.cpp",
    "LostOdysseyRecompSemantics/src/manager_construction.cpp",
    "LostOdysseyRecompSemantics/src/manager_lock.cpp",
    "LostOdysseyRecompSemantics/src/manager_storage.cpp",
]
SOURCE_FILES = [
    *COMPILE_SOURCES,
    *(f"LostOdysseyRecompSemantics/include/lo_semantics/{name}.h" for name in (
        "manager_init", "raw_allocation", "memory_services",
        "manager_construction", "manager_lock", "manager_storage", "guest_memory")),
    "LostOdysseyRecompSemantics/tests/manager_startup_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    HELPER_SOURCE,
    *(f"LostOdysseyRecompLib/ppc/ppc_recomp.{number}.cpp"
      for number, _, _ in FUNCTIONS.values()),
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_manager_startup.py",
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
                                   stderr=subprocess.STDOUT, timeout=180)
        output.write(completed.stdout)
        output.flush()
        print(completed.stdout, end="", flush=True)
        if completed.returncode:
            raise subprocess.CalledProcessError(completed.returncode, command)
        return completed.stdout


def verify_vtables(path: Path) -> dict:
    data = path.read_bytes()
    digest = sha(data)
    if digest != EXPECTED_IMAGE_SHA256 or len(data) != IMAGE_SIZE:
        raise ValueError("private unpacked image identity/size changed")
    expected = {
        "82002700": {56: "827C5D88", 60: "829664E8"},
        "8201DD90": {56: "827C5688", 60: "829664E8"},
    }
    for vtable, slots in expected.items():
        for offset, target in slots.items():
            position = int(vtable, 16) - IMAGE_BASE + offset
            actual = int.from_bytes(data[position:position + 4], "big") & ~3
            if actual != int(target, 16):
                raise ValueError(f"vtable target mismatch at {vtable}+{offset}")
    return {"image_path": str(path), "image_sha256": digest,
            "image_base": IMAGE_BASE, "image_size": len(data), "slots": expected}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE,
                        help="private unpacked image_disc1.bin with the pinned identity")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT.resolve()) or output.is_relative_to((Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    log = output / "manager_startup_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt-manager-startup.json"
    publish(receipt_path, {"status": "failed", "passed": False,
                           "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    vtable_evidence = verify_vtables(args.image.resolve())
    vtable_path = output / "manager_startup_vtables.json"
    publish(vtable_path, vtable_evidence)
    vtable_sha = sha(vtable_path.read_bytes())
    sources = {name: sha((ROOT / name).read_bytes())
               for name in dict.fromkeys(SOURCE_FILES)}
    functions = {address: extract((ROOT / f"LostOdysseyRecompLib/ppc/ppc_recomp.{number}.cpp").read_bytes(),
                                  f"sub_{address}", renamed)
                 for address, (number, renamed, _) in FUNCTIONS.items()}
    helpers = {name: extract((ROOT / HELPER_SOURCE).read_bytes(), name, name)
               for name in HELPERS}
    for address, (_, _, expected) in FUNCTIONS.items():
        if functions[address][1] != expected:
            raise ValueError(f"original PPC body changed at {address}: {functions[address][1]}")
    for name, expected in HELPERS.items():
        if helpers[name][1] != expected:
            raise ValueError(f"original ABI helper changed at {name}: {helpers[name][1]}")
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
        "__imp__RtlInitializeCriticalSection",
        "__imp__RtlTryEnterCriticalSection",
        "__imp__RtlLeaveCriticalSection")]
    generated = b"\n".join([b"\n".join(prelude),
                            *(body for body, _ in helpers.values()),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/manager_startup_oracle.cpp").read_bytes()])
    fixture = output / "manager_startup_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "manager_startup_oracle.exe"
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
        "suite": "manager-startup",
        "composition": "+".join(FUNCTIONS),
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "vtable_evidence_path": str(vtable_path),
        "vtable_evidence_sha256": vtable_sha,
        "vtable_image_path": vtable_evidence["image_path"],
        "vtable_image_sha256": vtable_evidence["image_sha256"],
        "original_functions_sha256": {address: body[1]
                                      for address, body in functions.items()},
        "abi_helpers_sha256": {name: body[1]
                               for name, body in helpers.items()},
        "source_sha256": sources,
        "generated_fixture_path": str(fixture),
        "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Native generated-PPC C++ composition, not raw XEX machine code or game runtime equivalence.",
            "Ten original bodies and six original ABI helpers compose; all three virtual targets are real recovered/original functions, read directly from a separately hash-pinned unpacked image.",
            "XEX and unpacked image identities are checked separately; this runner does not independently prove the image decryption/unpacking chain.",
            "Heap allocation, CRT retry/error calls, critical-section initialize/try/leave remain synthetic callbacks.",
            "Full ordinary object/lookup/bucket/pool bytes and high global/vtable pages, exact live frame slots at callbacks/final state, full r3 return and callback order are compared.",
            "Generic ABI stack scratch and complete volatile context are outside the semantic API and excluded from byte equality.",
            "No fault, MMIO, concurrency, or game-runtime equivalence.",
            "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
        ],
    }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        match = re.search(r"^PASS manager-startup (\d+)$", result, re.M)
        if match is None or int(match.group(1)) < 6:
            raise RuntimeError("manager startup completion line with >=6 cases required")
        receipt.update(status="passed", passed=True, cases=int(match.group(1)))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
