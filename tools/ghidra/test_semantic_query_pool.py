"""Run four independent generated-PPC differential suites on Windows.

Private generated function text is extracted only into an output directory
outside the synced repository. Each address receives its own evidence receipt.
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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-recovery-pool-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "827B72E8": ("LostOdysseyRecompLib/ppc/ppc_recomp.74.cpp", "oracle_InitializeQuerySlot"),
    "823CDCA8": ("LostOdysseyRecompLib/ppc/ppc_recomp.12.cpp", "oracle_ReleaseQuery"),
    "827C9D88": ("LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp", "oracle_AllocateDispatch"),
    "827C9DB0": ("LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp", "oracle_FreeDispatch"),
}
EXPECTED_FUNCTION_SHA256 = {
    "827B72E8": "77d23f48b546ded36816dbafb92638a914b627971897ee29c6671ef8241637e7",
    "823CDCA8": "35b69fbfd9e95e027277e84d8d6d2ecfa8c85b1182da711f639fa3f7853def8d",
    "827C9D88": "d901fcc09b8179cdfe820704bbdab99ee3be849acbef1b0533863620a0528a35",
    "827C9DB0": "d22f535f8cdc7641a5b6e032f68dfbaae55b838f46033e25e383b02929e23d06",
}
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
HELPERS = ("__savegprlr_25", "__restgprlr_25", "__savegprlr_28", "__restgprlr_28")
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/query_pool.cpp",
    "LostOdysseyRecompSemantics/src/query_release.cpp",
    "LostOdysseyRecompSemantics/src/allocation.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/query_pool.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/allocation.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/query_pool_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    HELPER_SOURCE,
    *(entry[0] for entry in FUNCTIONS.values()),
    "tools/tests/run.py",
    "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_query_pool.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode("ascii") + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected exactly one original generated PPC body for {symbol}; found {len(bodies)}")
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
    log = output / "query_pool_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_paths = {addr: output / f"receipt-{addr}.json" for addr in FUNCTIONS}
    for path in receipt_paths.values():
        publish(path, {"status": "failed", "passed": False,
                       "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}; expected {EXPECTED_XEX_SHA256}")
    source_hashes = {path: sha((ROOT / path).read_bytes()) for path in dict.fromkeys(SOURCE_FILES)}
    helper_bytes = (ROOT / HELPER_SOURCE).read_bytes()
    helper_bodies = [extract(helper_bytes, name, name) for name in HELPERS]
    extracted = {}
    for addr, (path, renamed) in FUNCTIONS.items():
        extracted[addr] = extract((ROOT / path).read_bytes(), f"sub_{addr}", renamed)
        if extracted[addr][1] != EXPECTED_FUNCTION_SHA256[addr]:
            raise ValueError(f"generated PPC body changed at {addr}: {extracted[addr][1]}")

    prelude = b'''#include "ppc_context.h"
PPC_EXTERN_FUNC(__savegprlr_25);
PPC_EXTERN_FUNC(__restgprlr_25);
PPC_EXTERN_FUNC(__savegprlr_28);
PPC_EXTERN_FUNC(__restgprlr_28);
PPC_EXTERN_FUNC(sub_827C9D88);
PPC_EXTERN_FUNC(sub_827C9DB0);
PPC_EXTERN_FUNC(sub_823EA178);
PPC_EXTERN_FUNC(sub_827C9A40);
PPC_EXTERN_FUNC(sub_827CA050);
PPC_EXTERN_FUNC(sub_827C9C60);
PPC_EXTERN_FUNC(sub_827CA0E8);
PPC_EXTERN_FUNC(oracle_InitializeQuerySlot);
PPC_EXTERN_FUNC(oracle_ReleaseQuery);
PPC_EXTERN_FUNC(oracle_AllocateDispatch);
PPC_EXTERN_FUNC(oracle_FreeDispatch);
'''
    generated = b"\n".join([prelude, *(body for body, _ in helper_bodies),
                            *(body for body, _ in extracted.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/query_pool_oracle.cpp").read_bytes()])
    fixture = output / "query_pool_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "query_pool_oracle.exe"
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
               str(fixture),
               *(str(ROOT / source) for source in SOURCE_FILES[:3]),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipts = {}
    for addr, (path, _) in FUNCTIONS.items():
        receipts[addr] = {
            "status": "failed", "passed": False, "function_address": addr,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "generated_ppc_path": str(ROOT / path),
            "generated_ppc_sha256": source_hashes[path],
            "original_function": f"sub_{addr}",
            "original_function_sha256": extracted[addr][1],
            "abi_helpers_sha256": dict(zip(HELPERS, (item[1] for item in helper_bodies))),
            "source_sha256": source_hashes,
            "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "boundaries": [
                "Native generated PPC C++ oracle, not raw XEX or gameplay equivalence.",
                "Only four dispatcher/query-pool functions are compared; allocator and notification callbacks are synthetic.",
                "823EA178 contains cache-range instructions omitted by generated PPC; notification comparison does not prove cache synchronization.",
                "No concurrent mutation, MMIO or real scene is modeled.",
                "The SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
                "Guest stack scratch 0x3E000..0x3FFFF is excluded from byte equality; r1, LR and r25..r31 are checked.",
            ],
        }
    try:
        run_logged(command, log, environment)
        result = run_logged([str(executable)], log)
        counts = {addr: int(count) for addr, count in re.findall(r"^PASS ([0-9A-F]{8}) (\d+)$", result, re.M)}
        if set(counts) != set(FUNCTIONS) or any(count < 256 for count in counts.values()):
            raise RuntimeError("four oracle completion lines with >=256 cases each are required")
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
