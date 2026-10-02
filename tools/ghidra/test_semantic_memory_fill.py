"""Compare recovered guest memset to the original generated PPC function.

The private original body is extracted into an output folder outside ownCloud;
the tracked test contains only synthetic guest-memory cases.
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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-memory-fill-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
GENERATED = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
EXPECTED_FUNCTION_SHA256 = "ae1680b659e636bf2ae47cee6875a5e73f80bf858deeacabc646ad949059239c"
SOURCE_FILES = [
    "LostOdysseyRecompSemantics/src/memory_fill.cpp",
    "LostOdysseyRecompSemantics/include/lo_semantics/memory_fill.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/detail/memory_fill_impl.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/ppc/memory_fill.h",
    "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
    "LostOdysseyRecompSemantics/tests/memory_fill_oracle.cpp",
    "LostOdysseyRecompSemantics/tests/memory_fill_store_trace.h",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    GENERATED,
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_memory_fill.py",
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(source: bytes) -> tuple[bytes, str]:
    pattern = rb"PPC_FUNC_IMPL\(__imp__sub_82B7BC40\) \{.*?\r?\n\}"
    bodies = re.findall(pattern, source, re.S)
    if len(bodies) != 1:
        raise ValueError(f"expected one original PPC body for 82B7BC40, got {len(bodies)}")
    original = bodies[0]
    return (original.replace(b"PPC_FUNC_IMPL(__imp__sub_82B7BC40)",
                             b"PPC_FUNC(oracle_FillGuestMemory)", 1), sha(original))


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
    log = output / "memory_fill_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt.json"
    publish(receipt_path, {"status": "failed", "passed": False,
                           "reason": "differential run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}; expected {EXPECTED_XEX_SHA256}")
    source_hashes = {name: sha((ROOT / name).read_bytes()) for name in SOURCE_FILES}
    body, body_sha = extract((ROOT / GENERATED).read_bytes())
    if body_sha != EXPECTED_FUNCTION_SHA256:
        raise ValueError(f"generated PPC body changed at 82B7BC40: {body_sha}")
    generated = b"#include \"memory_fill_store_trace.h\"\n#include \"ppc_context.h\"\n" + body + b"\n" + (
        ROOT / "LostOdysseyRecompSemantics/tests/memory_fill_oracle.cpp").read_bytes()
    fixture = output / "memory_fill_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "memory_fill_oracle.exe"
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
               "/I" + str(ROOT / "LostOdysseyRecompSemantics/tests"),
               str(fixture), str(ROOT / SOURCE_FILES[0]),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipt = {
        "status": "failed", "passed": False, "function_address": "82B7BC40",
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "generated_ppc_path": str(ROOT / GENERATED),
        "generated_ppc_sha256": source_hashes[GENERATED],
        "original_function": "sub_82B7BC40", "original_function_sha256": body_sha,
        "source_sha256": source_hashes,
        "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Native generated PPC C++ oracle, not raw XEX or gameplay equivalence.",
            "Bounded API and PPC adapter are both compared against the original; adapter full context and store addresses/widths/values/order match.",
            "Only ordinary committed guest memory is compared; real MMIO/concurrency effects and the live runtime call path remain untested.",
            "4 GiB is reserved virtually, with only low and final guest pages committed.",
            "SIMDE include tree and compiler installation are recorded by path, not fully hashed.",
        ],
    }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        count = re.search(r"^PASS 82B7BC40 (\d+) \(", result, re.M)
        if count is None or int(count.group(1)) < 1000:
            raise RuntimeError("oracle completion line missing or too few cases")
        receipt.update(status="passed", passed=True, cases=int(count.group(1)))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
