"""Shared native batch comparison support; read sources once and compile once."""

from __future__ import annotations

import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402


def extract_originals(entries: list[dict], ppc_root: Path) -> bytes:
    """Extract exact bodies at recorded source locations without rescanning a tree.

    Optional instruction_sequence values are compared with every source comment.
    Suite-specific validation remains responsible for accepting semantic patterns.
    """
    grouped: dict[str, list[dict]] = {}
    addresses = set()
    for entry in entries:
        address = entry["address"]
        if not re.fullmatch(r"[0-9A-F]{8}", address) or address in addresses:
            raise ValueError(f"invalid or duplicate entry address: {address}")
        addresses.add(address)
        name = Path(entry["generated_ppc_path"]).name
        if not re.fullmatch(r"ppc_recomp\.\d+\.cpp", name):
            raise ValueError(f"unexpected generated source: {name}")
        grouped.setdefault(name, []).append(entry)
    bodies = {}
    for name, members in grouped.items():
        lines = (Path(ppc_root) / name).read_bytes().splitlines(keepends=True)
        for entry in members:
            address = entry["address"]
            start = entry["line"] - 1
            expected = f"PPC_FUNC_IMPL(__imp__sub_{address}) {{".encode()
            if not 0 <= start < len(lines) or lines[start].rstrip() != expected:
                raise ValueError(f"source location changed: {address} {name}")
            end = start + 1
            while end < len(lines) and lines[end].rstrip(b"\r\n") != b"}":
                if lines[end].startswith(b"PPC_FUNC_IMPL("):
                    raise ValueError(f"unterminated body: {address}")
                end += 1
            if end == len(lines):
                raise ValueError(f"unterminated body: {address}")
            body = b"".join(lines[start:end + 1])
            if "instruction_sequence" in entry:
                comments = re.findall(rb"^\s*//\s*(.*?)\s*$", body, re.M)
                if [c.decode("ascii") for c in comments] != entry["instruction_sequence"]:
                    raise ValueError(f"instruction sequence changed: {address}")
            bodies[address] = body
    return b"\n".join(bodies[entry["address"]] for entry in entries)


def compile_and_run(suite: str, original_cpp: bytes, harness_cpp: bytes | str,
                    semantic_sources: list[Path | str], output: Path,
                    extra_include_dirs=(), semantic_library: Path | None = None,
                    msvc_runtime: str = "MD", native_environment: dict | None = None) -> dict:
    """Build one executable, run it once, and save a compact result and log."""
    if not re.fullmatch(r"[a-zA-Z0-9_-]+", suite):
        raise ValueError("suite must be a simple filename")
    output = Path(output).resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to(Path.home() / "ownCloud"):
        raise ValueError("batch output must be outside the checkout and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    fixture = output / f"{suite}.cpp"
    executable = output / f"{suite}.exe"
    if isinstance(harness_cpp, str):
        harness_cpp = harness_cpp.encode("utf-8")
    fixture.write_bytes(b'#include "ppc_context.h"\n#include <cstddef>\n' +
                        original_cpp + b"\n" + harness_cpp)
    result = {"suite": suite, "status": "failed", "phase": "compiler_setup",
              "msvc_runtime": msvc_runtime}
    result_path = output / f"{suite}-result.json"
    # Replace stale success before attempting a new build.
    result_path.write_text(json.dumps(result) + "\n", encoding="utf-8")
    try:
        if msvc_runtime not in ("MD", "MDd", "MT", "MTd"):
            raise ValueError(f"unsupported MSVC runtime: {msvc_runtime}")
        environment = native_environment if native_environment is not None else compiler_environment()
        search_path = next(v for k, v in environment.items() if k.upper() == "PATH")
        compiler = shutil.which("clang-cl", path=search_path)
        if compiler is None:
            raise RuntimeError("clang-cl unavailable after native compiler setup")
        includes = [*extra_include_dirs, ROOT / "LostOdysseyRecompLib/ppc",
                    ROOT / "tools/XenonRecomp/thirdparty/simde",
                    ROOT / "LostOdysseyRecompSemantics/include"]
        sources = [str(Path(p) if Path(p).is_absolute() else ROOT / p)
                   for p in semantic_sources]
        library = [str(Path(semantic_library).resolve())] if semantic_library else []
        if semantic_library and not Path(semantic_library).is_file():
            raise FileNotFoundError(f"semantics library missing: {semantic_library}")
        command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2",
                   "/" + msvc_runtime,
                   "-Wno-ignored-attributes", "-msse4.1",
                   *["/I" + str(p) for p in includes], str(fixture), *sources,
                   *library, "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
        with (output / f"{suite}.log").open("w", encoding="utf-8") as log:
            for name, args in (("compile", command), ("execute", [str(executable)])):
                result["phase"] = name
                started = time.perf_counter()
                try:
                    completed = subprocess.run(args, cwd=ROOT, env=environment,
                                               capture_output=True, text=True, timeout=300)
                finally:
                    result[name + "_seconds"] = round(time.perf_counter() - started, 3)
                output_text = completed.stdout + completed.stderr
                log.write(output_text)
                log.flush()
                print(output_text, end="", flush=True)
                if completed.returncode:
                    raise subprocess.CalledProcessError(completed.returncode, args)
                if name == "execute":
                    result["summary"] = output_text.strip()
        result["status"] = "passed"
        result["phase"] = "complete"
    except Exception as exc:
        result["error"] = f"{type(exc).__name__}: {exc}"
        raise
    finally:
        result_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result
