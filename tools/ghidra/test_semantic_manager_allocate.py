"""Native Windows original-PPC differential for two manager allocation wrappers."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-manager-allocate-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
IMAGE = ROOT / "LostOdysseyRecompLib/private/image_disc1.bin"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
EXPECTED_IMAGE_SHA256 = "cb756b46092e448923517bf660b44ad1a0651c2860882b43ae021f4ad4290f71"
PPC_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.14.cpp"
LOCK_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp"
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
FUNCTIONS = {
    "823F2670": ("oracle_AllocatePrimary",
                 "f9aecdadffe50bca906acc1df6d5819cdf30a9cd2b90fc70d70b8b05c6f35ea7"),
    "823F25E0": ("oracle_AllocateFallback",
                 "0ec348b7f24e004d6786003e2372370bc6649c91af6b601c7a919b7a0a241cdb"),
}
LOCK_BODY_SHA256 = "b4057a26d9fed0a651de1e70938e5aa49d8b9250b36b9a0492ef3d08acd04c4b"
HELPERS = {
    "__savegprlr_29": "2358ed1a9f8d6dd520b25318b93e2405baaa8d5e8126200fdd7c9b595182cd5f",
    "__restgprlr_29": "975666345b5ecf3ef1187776ad7dd5d1138adcfb34093dabc104620728387b3f",
}
SOURCES = [
    "LostOdysseyRecompSemantics/src/manager_allocate.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_allocate.h",
    "LostOdysseyRecompSemantics/src/manager_lock.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_lock.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/manager_allocate_oracle.cpp",
    PPC_SOURCE, LOCK_SOURCE, HELPER_SOURCE,
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_manager_allocate.py",
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
        result = subprocess.run(command, cwd=ROOT, env=environment, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=120)
        output.write(result.stdout)
        output.flush()
        print(result.stdout, end="", flush=True)
        if result.returncode:
            raise subprocess.CalledProcessError(result.returncode, command)
        return result.stdout


def image_target(image: bytes, slot: int) -> int:
    offset = slot - 0x82000000
    if offset < 0 or offset + 4 > len(image):
        raise ValueError(f"vtable slot outside pinned image: {slot:08X}")
    return int.from_bytes(image[offset:offset + 4], "big") & ~3


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    output = parser.parse_args().output.resolve()
    if output.is_relative_to(ROOT.resolve()) or output.is_relative_to((Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    log = output / "manager_allocate_oracle.log"
    log.write_text("", encoding="utf-8")
    paths = {address: output / f"receipt-{address}.json" for address in FUNCTIONS}
    paths["composition"] = output / "receipt-manager-allocate-composed.json"
    for path in paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    image = IMAGE.read_bytes()
    image_sha = sha(image)
    if image_sha != EXPECTED_IMAGE_SHA256 or len(image) != 0x13c0000:
        raise ValueError(f"private unpacked image identity changed: {image_sha}")
    slots = {
        "ORACLE_PRIMARY_ALLOC_METHOD": image_target(image, 0x82002704),
        "ORACLE_PRIMARY_WRAPPER_METHOD": image_target(image, 0x82002710),
        "ORACLE_FALLBACK_WRAPPER_METHOD": image_target(image, 0x8201dda0),
    }
    expected_slots = {
        "ORACLE_PRIMARY_ALLOC_METHOD": 0x82295b08,
        "ORACLE_PRIMARY_WRAPPER_METHOD": 0x823f2670,
        "ORACLE_FALLBACK_WRAPPER_METHOD": 0x823f25e0,
    }
    if slots != expected_slots:
        raise ValueError(f"manager vtable slots changed: {slots}")
    sources = {name: sha((ROOT / name).read_bytes()) for name in SOURCES}
    ppc = (ROOT / PPC_SOURCE).read_bytes()
    functions = {address: extract(ppc, f"sub_{address}", renamed)
                 for address, (renamed, _) in FUNCTIONS.items()}
    for address, (_, expected) in FUNCTIONS.items():
        if functions[address][1] != expected:
            raise ValueError(f"original PPC body changed at {address}: {functions[address][1]}")
    lock = extract((ROOT / LOCK_SOURCE).read_bytes(),
                   "sub_822958F8", "oracle_StoreLockAndWaitForEnter")
    if lock[1] != LOCK_BODY_SHA256:
        raise ValueError(f"original lock helper changed: {lock[1]}")
    helpers = {name: extract((ROOT / HELPER_SOURCE).read_bytes(), name, name)
               for name in HELPERS}
    for name, expected in HELPERS.items():
        if helpers[name][1] != expected:
            raise ValueError(f"original ABI helper changed at {name}: {helpers[name][1]}")

    prelude = [b'#define PPC_CALL_INDIRECT_FUNC(address) oracle_Indirect(ctx, base, (address))',
               b'#include "ppc_context.h"',
               b'void oracle_Indirect(PPCContext& ctx, uint8_t* base, uint32_t method);']
    prelude += [f"#define {name} 0x{value:08X}u".encode()
                for name, value in slots.items()]
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in HELPERS]
    prelude += [f"PPC_EXTERN_FUNC({renamed});".encode()
                for renamed, _ in FUNCTIONS.values()]
    prelude += [b'PPC_EXTERN_FUNC(oracle_StoreLockAndWaitForEnter);',
                b'PPC_EXTERN_FUNC(sub_822958F8);',
                b'PPC_EXTERN_FUNC(__imp__RtlTryEnterCriticalSection);',
                b'PPC_EXTERN_FUNC(__imp__RtlLeaveCriticalSection);']
    fixture_data = b"\n".join([b"\n".join(prelude),
                                *(body for body, _ in helpers.values()),
                                lock[0],
                                *(body for body, _ in functions.values()),
                                (ROOT / "LostOdysseyRecompSemantics/tests/manager_allocate_oracle.cpp").read_bytes()])
    fixture = output / "manager_allocate_oracle_generated.cpp"
    fixture.write_bytes(fixture_data)
    executable = output / "manager_allocate_oracle.exe"
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
               str(fixture), str(ROOT / SOURCES[0]), str(ROOT / SOURCES[2]),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    common = {
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "vtable_image_path": str(IMAGE), "vtable_image_sha256": image_sha,
        "vtable_slots": {name: f"{value:08X}" for name, value in slots.items()},
        "original_functions_sha256": {address: body[1]
                                      for address, body in functions.items()},
        "lock_helper_sha256": lock[1],
        "abi_helpers_sha256": {name: body[1] for name, body in helpers.items()},
        "source_sha256": sources,
        "generated_fixture_path": str(fixture),
        "generated_fixture_sha256": sha(fixture_data),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Compares generated PPC C++ bodies, not raw XEX execution or game runtime.",
            "Manager +4/+16 targets are read from a hash-pinned unpacked image; independent decryption provenance is not proven.",
            "Original and recovered 822958F8 are each invoked in fallback cases; original and recovered 823F2670 are each invoked inside composed fallback cases.",
            "Terminal +4 allocation method and native Try/Leave imports are synthetic callbacks.",
            "Full ordinary guest memory outside generic ABI scratch, live holder, full r3, callback order/args and owned memory snapshots are compared.",
            "Generic PPC ABI frame saves and backchain need a separate production adapter; concurrency, faults, MMIO and volatile-context effects are unproven.",
            "Compiler and SIMDE installation are recorded by path, not recursively hashed.",
        ],
    }
    receipts = {address: dict(common, status="failed", passed=False,
                              function_address=address,
                              original_function=f"sub_{address}",
                              original_function_sha256=functions[address][1],
                              generated_ppc_path=str(ROOT / PPC_SOURCE),
                              generated_ppc_sha256=sources[PPC_SOURCE])
                for address in FUNCTIONS}
    receipts["composition"] = dict(common, status="failed", passed=False,
                                   suite="manager-allocate-composed",
                                   composition="823F25E0+822958F8+823F2670")
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        required = {"823F2670": 24, "823F25E0": 24, "composition": 12}
        for key, minimum in required.items():
            label = "manager-allocate-composed" if key == "composition" else key
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
        for key, path in paths.items():
            publish(path, receipts[key])
    for path in paths.values():
        print(f"receipt: {path}")


if __name__ == "__main__":
    main()
