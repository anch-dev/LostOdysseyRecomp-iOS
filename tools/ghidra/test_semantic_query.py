"""Compile a private generated-PPC oracle against the tracked query recovery.

Only the generated test translation unit, binary, log and receipt leave this
script; none of the private generated function text is added to Git.
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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-recovery-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
SHARD = ROOT / "LostOdysseyRecompLib/ppc/ppc_recomp.74.cpp"
HELPERS = ROOT / "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
EXPECTED_FUNCTION_SHA256 = "bea9f96fe1dd8917d173f9848c1977ac71258c8844c4d81fbc79c9ae067ef51a"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes, symbol: str, new_symbol: str) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__" + symbol.encode("ascii") + rb"\) \{.*?\r?\n\}"
    matches = list(re.finditer(pattern, source, re.S))
    if len(matches) != 1:
        raise ValueError(f"expected exactly one original generated function {symbol}; found {len(matches)}")
    original = matches[0].group()
    renamed = original.replace(b"PPC_FUNC_IMPL(__imp__" + symbol.encode("ascii") + b")",
                               b"PPC_FUNC(" + new_symbol.encode("ascii") + b")", 1)
    return renamed, digest(original)


def run_logged(command: list[str], log: Path, env: dict | None = None) -> str:
    with log.open("a", encoding="utf-8") as stream:
        stream.write("+ " + subprocess.list2cmdline(command) + "\n")
        stream.flush()
        completed = subprocess.run(command, cwd=ROOT, env=env, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stream.write(completed.stdout)
        stream.flush()
        print(completed.stdout, end="", flush=True)
        if completed.returncode:
            raise subprocess.CalledProcessError(completed.returncode, command)
        return completed.stdout


def publish_receipt(path: Path, receipt: dict) -> None:
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT.resolve()) or output.is_relative_to((Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    log = output / "query_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt.json"
    publish_receipt(receipt_path, {"status": "failed", "passed": False,
                                   "reason": "oracle run has not completed", "log_path": str(log)})

    xex_sha = digest(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}; expected {EXPECTED_XEX_SHA256}")
    shard = SHARD.read_bytes()
    helpers = HELPERS.read_bytes()
    function, function_sha = extract(shard, "sub_827B7408", "oracle_CreateType9Query")
    if function_sha != EXPECTED_FUNCTION_SHA256:
        raise ValueError(f"generated PPC function body changed: {function_sha}; expected {EXPECTED_FUNCTION_SHA256}")
    save, save_sha = extract(helpers, "__savegprlr_27", "__savegprlr_27")
    restore, restore_sha = extract(helpers, "__restgprlr_27", "__restgprlr_27")
    prelude = b'''#include "ppc_context.h"\n
PPC_EXTERN_FUNC(__savegprlr_27);\n
PPC_EXTERN_FUNC(__restgprlr_27);\n
PPC_EXTERN_FUNC(sub_827C9D88);\n
PPC_EXTERN_FUNC(sub_827B72E8);\n
PPC_EXTERN_FUNC(sub_823CDCA8);\n
'''
    source = b"\n".join([prelude, save, restore, function,
                         (ROOT / "LostOdysseyRecompSemantics/tests/query_oracle.cpp").read_bytes()])
    fixture = output / "query_oracle_generated.cpp"
    fixture.write_bytes(source)
    executable = output / "query_oracle.exe"
    environment = compiler_environment()
    search_path = next(value for key, value in environment.items() if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    if not compiler:
        raise RuntimeError("clang-cl missing after native Windows compiler setup")
    command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/Od", "/MD",
               "-Wno-ignored-attributes", "-msse4.1",
               "/I" + str(ROOT / "LostOdysseyRecompLib/ppc"),
               "/I" + str(ROOT / "tools/XenonRecomp/thirdparty/simde"),
               "/I" + str(ROOT / "LostOdysseyRecompSemantics/include"),
               str(fixture), str(ROOT / "LostOdysseyRecompSemantics/src/query.cpp"),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    tracked_sources = [
        "LostOdysseyRecompSemantics/src/query.cpp",
        "LostOdysseyRecompSemantics/include/lo_semantics/query.h",
        "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
        "LostOdysseyRecompSemantics/tests/query_oracle.cpp",
        "LostOdysseyRecompLib/ppc/ppc_context.h",
        "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp",
        "tools/ghidra/test_semantic_query.py",
    ]
    receipt = {
        "status": "failed", "passed": False, "function_address": "827B7408",
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "generated_ppc_path": str(SHARD), "generated_ppc_sha256": digest(shard),
        "original_function": "sub_827B7408", "original_function_sha256": function_sha,
        "abi_helpers_sha256": {"savegprlr_27": save_sha, "restgprlr_27": restore_sha},
        "source_sha256": {path: digest((ROOT / path).read_bytes()) for path in tracked_sources},
        "generated_fixture_path": str(fixture), "generated_fixture_sha256": digest(source),
        "compiler": compiler, "compile_command": command,
        "log_path": str(log),
        "boundaries": [
            "Native execution of generated PPC C++; not raw XEX or runtime equivalence.",
            "Only function 827B7408 is recovered; allocator, initialization and release use synthetic callbacks.",
            "No concurrent guest mutations or MMIO are modeled.",
            "Guest stack scratch 0x3E000..0x3FFFF is excluded from byte equality; r1, LR and r27..r31 are checked.",
        ],
    }
    try:
        run_logged(command, log, environment)
        result = run_logged([str(executable)], log)
        count = re.search(r"^PASS: (\d+) PPC/recovered differential cases \(", result, re.M)
        if count is None or int(count.group(1)) < 256:
            raise RuntimeError("oracle completion line missing or too few differential cases")
        receipt["status"] = "passed"
        receipt["passed"] = True
        receipt["cases"] = int(count.group(1))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish_receipt(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
