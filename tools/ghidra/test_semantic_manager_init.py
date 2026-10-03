"""Native Windows original-PPC differential for 827C5F38 manager initialization."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-manager-init-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTION_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp"
EXPECTED_FUNCTION_SHA256 = "48e4b60654cdc9db936d96a4363ab85abc21753c32383593b62fbc24720e71a3"
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/manager_init.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/manager_init.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/manager_init_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    FUNCTION_SOURCE,
    "tools/tests/run.py",
    "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_manager_init.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes) -> tuple[bytes, str]:
    body = re.findall(rb"PPC_FUNC_IMPL\(__imp__sub_827C5F38\) \{.*?\r?\n\}", source, re.S)
    if len(body) != 1:
        raise ValueError(f"expected one original generated PPC body, got {len(body)}")
    return (body[0].replace(b"PPC_FUNC_IMPL(__imp__sub_827C5F38)",
                            b"PPC_FUNC(oracle_InitializeManager)", 1), sha(body[0]))


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
    log = output / "manager_init_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt-827C5F38.json"
    publish(receipt_path, {"status": "failed", "passed": False,
                           "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    sources = {name: sha((ROOT / name).read_bytes()) for name in SOURCE_FILES}
    body, body_sha = extract((ROOT / FUNCTION_SOURCE).read_bytes())
    if body_sha != EXPECTED_FUNCTION_SHA256:
        raise ValueError(f"original generated PPC body changed: {body_sha}")
    prelude = b'''#define PPC_CALL_INDIRECT_FUNC(address) oracle_Indirect(ctx, base, (address))
#include "ppc_context.h"
PPC_EXTERN_FUNC(oracle_InitializeManager);
PPC_EXTERN_FUNC(sub_823ACBD0);
PPC_EXTERN_FUNC(sub_827C5970);
PPC_EXTERN_FUNC(sub_827C4ED0);
void oracle_Indirect(PPCContext& ctx, uint8_t* base, uint32_t address);
'''
    generated = b"\n".join([prelude, body,
                            (ROOT / "LostOdysseyRecompSemantics/tests/manager_init_oracle.cpp").read_bytes()])
    fixture = output / "manager_init_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "manager_init_oracle.exe"
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
    receipt = {
        "status": "failed", "passed": False, "function_address": "827C5F38",
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "generated_ppc_path": str(ROOT / FUNCTION_SOURCE),
        "generated_ppc_sha256": sources[FUNCTION_SOURCE],
        "original_function": "sub_827C5F38", "original_function_sha256": body_sha,
        "source_sha256": sources,
        "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Native generated-PPC C++ oracle, not raw XEX machine code or game runtime equivalence.",
            "823ACBD0 allocation, 827C5970/827C4ED0 constructors, and vtable slots +60/+56 are synthetic callbacks.",
            "Callback arguments, event order, and before/after ordinary-memory hashes are compared; final committed guest bytes and full r3 return are compared exactly.",
            "Covers null and high-half-only allocation, constructor results, success/fallback, global/local mutation and frame+80 aliasing the manager global.",
            "ABI saves/backchain are replayed by the adapter; live +80 local is implemented in semantic code.",
            "No full volatile-context, fault, MMIO, concurrency, or game-runtime equivalence.",
            "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
        ],
    }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        count = re.search(r"^PASS 827C5F38 (\d+)$", result, re.M)
        if count is None or int(count.group(1)) < 50:
            raise RuntimeError("manager init oracle completion line with >=50 cases required")
        receipt.update(status="passed", passed=True, cases=int(count.group(1)))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
