"""Native Windows original-PPC composition check for a heap allocate/free cycle."""

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

DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-heap-lifecycle-tests"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
FUNCTIONS = {
    "823ACCB0": ("11", "oracle_AllocateHeapBlock"),
    "823AD544": ("11", "oracle_LeaveHeapAllocateCriticalSection"),
    "823ADE28": ("11", "oracle_FreeHeapBlock"),
    "823AE0BC": ("11", "oracle_LeaveHeapCriticalSection"),
    "823AE108": ("11", "oracle_CoalesceFreeBlocks"),
    "827CC428": ("76", "oracle_GrowHeap"),
    "827CB778": ("76", "oracle_ExtendHeapSegment"),
    "827CC2C0": ("76", "oracle_InitializeHeapSegment"),
    "827CC668": ("76", "oracle_DecommitFreeBlock"),
    "827CB498": ("76", "oracle_AllocateRangeNode"),
    "827CB658": ("76", "oracle_InsertRangeRecord"),
    "827CBA60": ("76", "oracle_InsertFreeBlocks"),
    "82B7BC40": ("175", "oracle_FillGuestMemory"),
}
EXPECTED_FUNCTION_SHA256 = {
    "823ACCB0": "77568b14acdd8ce66212816e1b8821ee4a49b2989df305e55854297a32f1e3fb",
    "823AD544": "456802047bc8b299c5b4f6b7df3059c25beab44198abb2df70a8060b513bf27c",
    "823ADE28": "68bfcc98d18c87f61d719e864851aba9b900ad8914c2559f9b85793141d2a247",
    "823AE0BC": "d091df6c37db36c7a6ff74634ded0143b700f536dec3b059badaff537130516b",
    "823AE108": "969dade741098ec3c1db3688109c352659cce813b7d62b9a8e1307e4ea8c0389",
    "827CC428": "8f5ab5121338282af8368fd6f3db0b604e73223963caac8005193b9b842774a8",
    "827CB778": "3244a4fbf2a825ab4e48715655b4c51a97a148726ff0b14f2ed6aa0d7d2d92ca",
    "827CC2C0": "c14253ec6514cb5ce1882b60efe2148c0a2aa97c2c7a26ec7a7bd98429dd3ac9",
    "827CC668": "c20186cce8b9ff2cf242de673a71bfda8ac06ee00155be01cafbeae0a1e5703d",
    "827CB498": "a8a9018af697940f1532ac1b3826313d6cf3d8d06d0c996de539cdebdd84986c",
    "827CB658": "c862ff255d345d90bcd94ff36ad1cf6007e214e527bc08eb6463e972cdca02eb",
    "827CBA60": "e4df966d6628870c304f3eed2d338a962e7c2f7c7d0ac34632018426548cf060",
    "82B7BC40": "ae1680b659e636bf2ae47cee6875a5e73f80bf858deeacabc646ad949059239c",
}
HELPER_SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp"
HELPERS = tuple(f"__{kind}gprlr_{n}" for n in (19, 22, 25, 27, 28)
                for kind in ("save", "rest"))
SOURCES = [
    *(f"LostOdysseyRecompSemantics/src/{name}.cpp" for name in
      ("heap_allocate", "heap_free", "heap_growth", "heap_segment",
       "heap_decommit", "heap_ranges", "heap", "memory_fill")),
    *(f"LostOdysseyRecompSemantics/include/lo_semantics/{name}.h" for name in
      ("heap_allocate", "heap_free", "heap_growth", "heap_segment",
       "heap_decommit", "heap_ranges", "heap", "memory_fill",
       "detail/memory_fill_impl", "guest_memory")),
    "LostOdysseyRecompSemantics/tests/heap_lifecycle_oracle.cpp",
    "LostOdysseyRecompLib/ppc/ppc_context.h",
    "LostOdysseyRecompLib/ppc/ppc_config.h",
    *(f"LostOdysseyRecompLib/ppc/ppc_recomp.{file}.cpp" for file in ("11", "76", "175")),
    "tools/tests/run.py", "tools/setup_windows.bat",
    "tools/ghidra/test_semantic_heap_lifecycle.py",
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


def publish(path: Path, value: dict) -> None:
    pending = path.with_suffix(".json.tmp")
    pending.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
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
    log = output / "heap_lifecycle_oracle.log"
    log.write_text("", encoding="utf-8")
    receipt_path = output / "receipt.json"
    publish(receipt_path, {"status": "failed", "passed": False,
                           "reason": "lifecycle run has not completed", "log_path": str(log)})

    xex_sha = sha(XEX.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        raise ValueError(f"private XEX identity changed: {xex_sha}")
    hashes = {name: sha((ROOT / name).read_bytes()) for name in dict.fromkeys(SOURCES)}
    helper_source = (ROOT / HELPER_SOURCE).read_bytes()
    helpers = {name: extract(helper_source, name, name) for name in HELPERS}
    functions = {addr: extract((ROOT / f"LostOdysseyRecompLib/ppc/ppc_recomp.{file}.cpp").read_bytes(),
                               f"sub_{addr}", renamed)
                 for addr, (file, renamed) in FUNCTIONS.items()}
    for addr, (_, digest) in functions.items():
        if digest != EXPECTED_FUNCTION_SHA256[addr]:
            raise ValueError(f"original generated PPC body changed at {addr}: {digest}")
    prelude = [b'#include "ppc_context.h"',
               b'void oracle_CommitIndirect(PPCContext&, std::uint8_t*, std::uint32_t);',
               b'#undef PPC_CALL_INDIRECT_FUNC',
               b'#define PPC_CALL_INDIRECT_FUNC(x) oracle_CommitIndirect(ctx, base, static_cast<std::uint32_t>(x) & ~3u)']
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in HELPERS]
    prelude += [f"PPC_EXTERN_FUNC({renamed});".encode()
                for _, renamed in FUNCTIONS.values()]
    prelude += [f"PPC_EXTERN_FUNC(sub_{addr});".encode() for addr in FUNCTIONS]
    prelude += [f"PPC_EXTERN_FUNC({name});".encode() for name in
                ("__imp__KeGetCurrentProcessType", "__imp__KeBugCheckEx",
                 "__imp__RtlEnterCriticalSection", "__imp__RtlLeaveCriticalSection",
                 "__imp__NtAllocateVirtualMemory", "__imp__NtFreeVirtualMemory",
                 "__imp__RtlCompareMemoryUlong", "__imp__RtlRaiseException")]
    generated = b"\n".join([b"\n".join(prelude),
                            *(body for body, _ in helpers.values()),
                            *(body for body, _ in functions.values()),
                            (ROOT / "LostOdysseyRecompSemantics/tests/heap_lifecycle_oracle.cpp").read_bytes()])
    fixture = output / "heap_lifecycle_oracle_generated.cpp"
    fixture.write_bytes(generated)
    executable = output / "heap_lifecycle_oracle.exe"
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
               *(str(ROOT / name) for name in SOURCES[:8]),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    receipt = {
        "status": "failed", "passed": False, "suite": "heap_lifecycle_composition",
        "xex_path": str(XEX), "xex_sha256": xex_sha,
        "original_functions_sha256": {addr: body[1] for addr, body in functions.items()},
        "abi_helpers_sha256": {name: body[1] for name, body in helpers.items()},
        "source_sha256": hashes,
        "generated_fixture_path": str(fixture), "generated_fixture_sha256": sha(generated),
        "compiler": compiler, "compile_command": command, "log_path": str(log),
        "boundaries": [
            "Generated original PPC C++ is not raw XEX machine-code equivalence.",
            "This is an independent composition fixture, not one of the ordinary per-function receipts.",
            "Kernel VM, process, lock and exception services are synthetic; decommit physical page state is not modeled.",
            "Four-MiB low guest window only; no game runtime, concurrent access, MMIO or damaged heap graph.",
            "Guest stack scratch is reset before each top-level operation; ordinary heap state persists across allocate/free/reallocate.",
            "ABI spill scratch is excluded from full-byte equality; selected live frame locals and Initialize's committed-end spill into its Grow caller are checked.",
            "Original PPC r1, LR and nonvolatile GPR restoration is checked; this does not validate a registered runtime adapter.",
            "SIMDE header tree and native compiler installation are identified by paths, not fully hashed.",
        ],
    }
    try:
        logged(command, log, environment)
        result = logged([str(executable)], log)
        matches = re.findall(r"^PASS LIFECYCLE (\d+) events=(\d+)$", result, re.M)
        if len(matches) != 1 or int(matches[0][0]) < 1 or int(matches[0][1]) < 8:
            raise RuntimeError("lifecycle completion line and boundary events are required")
        receipt.update(status="passed", passed=True, cases=int(matches[0][0]),
                       events=int(matches[0][1]))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
