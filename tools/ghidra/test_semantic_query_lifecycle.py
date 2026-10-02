"""Compare a complete type-9 query lifecycle with three original generated PPC bodies.

Generated PPC text, executable, log and receipt stay outside the synced tree.
This composition check supplements the independent function oracles.
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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-query-lifecycle-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "827B7408": ("LostOdysseyRecompLib/ppc/ppc_recomp.74.cpp", "oracle_CreateType9Query",
                 "bea9f96fe1dd8917d173f9848c1977ac71258c8844c4d81fbc79c9ae067ef51a"),
    "827B72E8": ("LostOdysseyRecompLib/ppc/ppc_recomp.74.cpp", "sub_827B72E8",
                 "77d23f48b546ded36816dbafb92638a914b627971897ee29c6671ef8241637e7"),
    "823CDCA8": ("LostOdysseyRecompLib/ppc/ppc_recomp.12.cpp", "sub_823CDCA8",
                 "35b69fbfd9e95e027277e84d8d6d2ecfa8c85b1182da711f639fa3f7853def8d"),
}
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
HELPERS = ("__savegprlr_25", "__restgprlr_25", "__savegprlr_27",
           "__restgprlr_27", "__savegprlr_28", "__restgprlr_28")
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/query.cpp",
    "LostOdysseyRecompSemantics/src/query_pool.cpp",
    "LostOdysseyRecompSemantics/src/query_release.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/query.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/query_pool.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/query_lifecycle.h",
    "LostOdysseyRecompSemantics/tests/query_lifecycle_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    HELPER_SOURCE,
    *(entry[0] for entry in FUNCTIONS.values()),
    "tools/tests/run.py",
    "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_query_lifecycle.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, renamed: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode("ascii") + rb"\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one generated PPC body for {symbol}; found {len(bodies)}")
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
    log = output / "query_lifecycle_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt-827B7408-lifecycle.json"
    publish(receipt_path, {"status": "failed", "passed": False,
                           "reason": "lifecycle differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}; expected {EXPECTED_XEX_SHA256}")
    source_hashes = {path: sha((ROOT / path).read_bytes()) for path in dict.fromkeys(SOURCE_FILES)}
    helper_bytes = (ROOT / HELPER_SOURCE).read_bytes()
    helpers = [extract(helper_bytes, name, name) for name in HELPERS]
    extracted = {}
    for address, (path, renamed, expected_hash) in FUNCTIONS.items():
        extracted[address] = extract((ROOT / path).read_bytes(), f"sub_{address}", renamed)
        if extracted[address][1] != expected_hash:
            raise ValueError(f"generated PPC body changed at {address}: {extracted[address][1]}")

    prelude = b'''#include "ppc_context.h"
PPC_EXTERN_FUNC(__savegprlr_25);
PPC_EXTERN_FUNC(__restgprlr_25);
PPC_EXTERN_FUNC(__savegprlr_27);
PPC_EXTERN_FUNC(__restgprlr_27);
PPC_EXTERN_FUNC(__savegprlr_28);
PPC_EXTERN_FUNC(__restgprlr_28);
PPC_EXTERN_FUNC(sub_827C9D88);
PPC_EXTERN_FUNC(sub_827C9DB0);
PPC_EXTERN_FUNC(sub_823EA178);
PPC_EXTERN_FUNC(sub_827B72E8);
PPC_EXTERN_FUNC(sub_823CDCA8);
PPC_EXTERN_FUNC(oracle_CreateType9Query);
'''
    generated = b"\n".join([prelude, *(body for body, _ in helpers),
                            *(body for body, _ in extracted.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/query_lifecycle_oracle.cpp").read_bytes()])
    fixture = output / "query_lifecycle_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "query_lifecycle_oracle.exe"
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
    receipt = {
        "status": "failed", "passed": False, "function_address": "827B7408",
        "evidence_scope": "composed create-initialize-release lifecycle; supplements independent function oracles",
        "composed_functions": list(FUNCTIONS),
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "original_function_sha256": {address: extracted[address][1] for address in FUNCTIONS},
        "abi_helpers_sha256": dict(zip(HELPERS, (item[1] for item in helpers))),
        "source_sha256": source_hashes,
        "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Original generated PPC bodies are executed natively, not the XEX or a game scene.",
            "Underlying allocation, free and cache notification callbacks are deterministic synthetic services.",
            "The original cache flush instructions are omitted by generated PPC; notification traces do not prove hardware cache behavior.",
            "Guest stack scratch 0x3E000..0x3FFFF is excluded from byte equality; r1, LR and r25..r31 are checked.",
            "The SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
        ],
    }
    try:
        run_logged(command, log, environment)
        result = run_logged([str(executable)], log)
        match = re.search(r"^PASS lifecycle (\d+) ", result, re.M)
        if match is None or int(match.group(1)) < 74:
            raise RuntimeError("lifecycle oracle completion line missing or too few cases")
        receipt.update(status="passed", passed=True, cases=int(match.group(1)))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
