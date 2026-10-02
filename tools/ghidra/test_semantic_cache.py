"""Compare recovered cache range operations with instrumented original PPC.

Generated PPC drops dcbf/sync. This oracle emits trace events at those exact
instruction comments; it validates their control-flow order, not cache hardware.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

from test_semantic_query import (ROOT, XEX, EXPECTED_XEX_SHA256, compiler_environment,
                                 extract, publish_receipt, run_logged)


SHARD = ROOT / "LostOdysseyRecompLib/ppc/ppc_recomp.14.cpp"
EXPECTED_FUNCTION = "bd2b0c0bcfb9890beb8105c7305e5ee79eb26eb39e378fa6ee688b8d5afcca51"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-cache-tests")
    output = parser.parse_args().output.resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to((Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    receipt_path = output / "receipt.json"
    log = output / "cache_oracle.log"
    receipt = {"function_address": "823EA178", "status": "failed", "passed": False,
               "reason": "test has not completed"}
    publish_receipt(receipt_path, receipt)
    log.write_text("", encoding="utf-8")
    try:
        xex_sha = digest(XEX.read_bytes())
        if xex_sha != EXPECTED_XEX_SHA256:
            raise ValueError("private XEX identity changed")
        shard = SHARD.read_bytes()
        body, original_sha = extract(shard, "sub_823EA178", "oracle_FlushDataCacheRange")
        if original_sha != EXPECTED_FUNCTION:
            raise ValueError("original cache function changed")
        for comment, statement, expected in (
            (b"// dcbf r0,r11", b"TraceFlush(ctx.r11.u32);", 2),
            (b"// dcbf r8,r11", b"TraceFlush(ctx.r8.u32 + ctx.r11.u32);", 7),
            (b"// sync ", b"TraceSync();", 1),
        ):
            if body.count(comment) != expected:
                raise ValueError("cache instruction instrumentation count changed")
            body = body.replace(comment, comment + b"\n\t" + statement)
        fixture = output / "cache_oracle_generated.cpp"
        fixture.write_bytes(b'#include "ppc_context.h"\n'
                            b'static void TraceFlush(std::uint32_t);\nstatic void TraceSync();\n' + body + b"\n" +
                            (ROOT / "LostOdysseyRecompSemantics/tests/cache_oracle.cpp").read_bytes())
        env = compiler_environment()
        search_path = next(value for key, value in env.items() if key.upper() == "PATH")
        compiler = shutil.which("clang-cl", path=search_path)
        if not compiler:
            raise RuntimeError("native Windows clang-cl is unavailable")
        executable = output / "cache_oracle.exe"
        command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/Od", "/MD",
                   "-Wno-ignored-attributes", "-msse4.1",
                   "/I" + str(ROOT / "LostOdysseyRecompLib/ppc"),
                   "/I" + str(ROOT / "tools/XenonRecomp/thirdparty/simde"),
                   "/I" + str(ROOT / "LostOdysseyRecompSemantics/include"),
                   str(fixture), str(ROOT / "LostOdysseyRecompSemantics/src/cache.cpp"),
                   "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
        sources = ["LostOdysseyRecompSemantics/src/cache.cpp",
                   "LostOdysseyRecompSemantics/include/lo_semantics/cache.h",
                   "LostOdysseyRecompSemantics/include/lo_semantics/guest_memory.h",
                   "LostOdysseyRecompSemantics/tests/cache_oracle.cpp",
                   "LostOdysseyRecompLib/ppc/ppc_context.h",
                   "LostOdysseyRecompLib/ppc/ppc_config.h",
                   "tools/ghidra/test_semantic_cache.py", "tools/ghidra/test_semantic_query.py",
                   "tools/tests/run.py", "tools/setup_windows.bat"]
        receipt.update({
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "original_function": "sub_823EA178", "original_function_sha256": original_sha,
            "generated_ppc_path": str(SHARD), "generated_ppc_sha256": digest(shard),
            "generated_fixture_path": str(fixture), "generated_fixture_sha256": digest(fixture.read_bytes()),
            "source_sha256": {name: digest((ROOT / name).read_bytes()) for name in sources},
            "compiler": compiler, "compile_command": command, "log_path": str(log),
            "oracle_kind": "Original PPC control flow with dcbf/sync instruction-event instrumentation",
            "boundaries": [
                "Cache instruction order/address traces only; no actual hardware cache or memory-ordering effects.",
                "1040 complete traces plus two bounded prefixes of the original enormous unsigned loops.",
                "The original generated PPC omits dcbf/sync; the runner inserts trace events at the pinned instruction comments.",
                "No runtime adapter or gameplay validation.",
                "The compiler and SIMDE include paths are recorded, but their entire installed contents are not hashed.",
            ],
        })
        run_logged(command, log, env)
        result = run_logged([str(executable)], log)
        match = re.search(r"^PASS: (\d+) cache trace cases", result, re.M)
        if not match or int(match.group(1)) != 1042:
            raise ValueError("cache oracle did not report all expected cases")
        receipt.update(status="passed", passed=True, cases=int(match.group(1)))
        receipt.pop("reason", None)
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish_receipt(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
