"""Verify exact leaf templates and batch-differential every original PPC entry."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sqlite3
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "tests"))
from run import compiler_environment  # noqa: E402

DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-leaf-family-tests"
INVENTORY = ROOT / "out/function-inventory/exact-body-families.json"
FAMILY_MAP = ROOT / "LostOdysseyRecompSemantics/leaf_families.json"
XEX = ROOT / "LostOdysseyRecompLib/private/disc1/default.xex"
IMAGE = ROOT / "LostOdysseyRecompLib/private/image_disc1.bin"
EXPECTED_XEX_SHA256 = "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2"
EXPECTED_IMAGE_SHA256 = "cb756b46092e448923517bf660b44ad1a0651c2860882b43ae021f4ad4290f71"
IMAGE_BASE = 0x82000000
CASES_PER_ENTRY = 5

# Exact original generated-body spellings, including every PPC instruction
# comment and the no-op self-move. Anything else fails closed.
TEMPLATES = {
    "blr": {
        "body_sha256": "bd50cd3d52456157ff4861b081252fb90f2add6e53f3602302bf32d423e13aa3",
        "image_bytes": "4e800020",
        "semantic": "PreserveR3",
        "lines": (b"\tPPC_FUNC_PROLOGUE();", b"\t// blr", b"\treturn;", b"}"),
    },
    "li_one_blr": {
        "body_sha256": "d51aa3e2c7115414e8f67980166b9b1bfdeeda7682d8c0a71b497cd853d4188d",
        "image_bytes": "386000014e800020",
        "semantic": "ReturnOne",
        "lines": (b"\tPPC_FUNC_PROLOGUE();", b"\t// li r3,1", b"\tctx.r3.s64 = 1;",
                  b"\t// blr", b"\treturn;", b"}"),
    },
    "li_one_mr_r8_blr": {
        "body_sha256": "236bf4a5a28e63bf9dfe535ce17494d26d7251006219f203dd9b0f2d4d63bdda",
        "image_bytes": "386000017d0843784e800020",
        "semantic": "ReturnOne",
        "lines": (b"\tPPC_FUNC_PROLOGUE();", b"\t// li r3,1", b"\tctx.r3.s64 = 1;",
                  b"\t// mr r8,r8", b"\tctx.r8.u64 = ctx.r8.u64;",
                  b"\t// blr", b"\treturn;", b"}"),
    },
}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def publish(path: Path, data: dict) -> None:
    pending = path.with_name(path.name + ".tmp")
    pending.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    pending.replace(path)


def catalog_locations(catalog: Path, addresses: set[str]) -> dict[str, tuple[str, int]]:
    db = sqlite3.connect("file:" + str(catalog) + "?mode=ro", uri=True)
    try:
        locations = {
            address.upper(): (path, line)
            for address, path, line in db.execute(
                "SELECT address,path,line FROM evidence WHERE kind='generated_ppc'"
            ) if address.upper() in addresses
        }
        xex = db.execute("SELECT value FROM metadata WHERE key='xex_sha256'").fetchone()
    finally:
        db.close()
    if set(locations) != addresses:
        raise ValueError("catalog lacks or duplicates target generated PPC entries")
    if xex is None or xex[0].lower() != EXPECTED_XEX_SHA256:
        raise ValueError("catalog XEX identity differs from pinned private XEX")
    return locations


def source_bodies(
    ppc_root: Path, locations: dict[str, tuple[str, int]], inventory: dict,
    templates_by_address: dict[str, str],
) -> tuple[dict[str, bytes], dict[str, str], dict[str, str]]:
    by_source: dict[str, list[tuple[str, int]]] = {}
    for address, (relative, line) in locations.items():
        if not re.fullmatch(r"LostOdysseyRecompLib/ppc/ppc_recomp\.\d+\.cpp", relative):
            raise ValueError(f"unexpected generated source path at {address}: {relative}")
        by_source.setdefault(relative, []).append((address, line))
    bodies: dict[str, bytes] = {}
    original_sha: dict[str, str] = {}
    source_sha: dict[str, str] = {}
    for relative, entries in sorted(by_source.items()):
        path = ppc_root / Path(relative).name
        data = path.read_bytes()
        digest = sha(data)
        if digest != inventory["source_sha256"].get(relative):
            raise ValueError(f"generated PPC source hash changed: {relative}: {digest}")
        source_sha[relative] = digest
        lines = data.splitlines(keepends=True)
        for address, line in entries:
            header = f"PPC_FUNC_IMPL(__imp__sub_{address}) {{".encode()
            if line < 1 or line > len(lines) or lines[line - 1].rstrip(b"\r\n") != header:
                raise ValueError(f"catalog source/line mismatch: {address} {relative}:{line}")
            template = TEMPLATES[templates_by_address[address]]
            end = line + len(template["lines"])
            if end > len(lines):
                raise ValueError(f"truncated original PPC body: {address}")
            actual = tuple(raw.rstrip(b"\r\n").rstrip(b" \t") for raw in lines[line:end])
            if actual != template["lines"]:
                raise ValueError(f"unapproved original PPC instruction/body at {address}")
            normalized = b"\n".join(actual) + b"\n"
            if sha(normalized) != template["body_sha256"]:
                raise ValueError(f"exact body fingerprint mismatch: {address}")
            original = b"".join(lines[line - 1:end])
            bodies[address] = original
            original_sha[address] = sha(original)
    if set(bodies) != set(locations):
        raise ValueError("not all target original PPC bodies extracted")
    return bodies, original_sha, source_sha


def make_fixture(entries: list[dict], bodies: dict[str, bytes]) -> bytes:
    head = b'#include "ppc_context.h"\n#include <cstddef>\n'
    originals = b"\n".join(bodies[entry["address"]] for entry in entries)
    table = [
        b"enum class LeafMode { PreserveR3, ReturnOne };",
        b"struct LeafEntry { unsigned address; void (*original)(PPCContext&, uint8_t*); LeafMode mode; };",
        b"static const LeafEntry kLeafEntries[] = {",
    ]
    for entry in entries:
        address = entry["address"]
        semantic = TEMPLATES[entry["template"]]["semantic"]
        table.append(f"{{0x{address}u, &__imp__sub_{address}, LeafMode::{semantic}}},".encode())
    table.append(b"};")
    test = (ROOT / "LostOdysseyRecompSemantics/tests/leaf_family_oracle.cpp").read_bytes()
    return head + originals + b"\n" + b"\n".join(table) + b"\n" + test


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT,
                        help="read-only complete generated PPC directory")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--write-map", action="store_true",
                        help="write verified tracked address/family map")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT.resolve()) or output.is_relative_to((Path.home() / "ownCloud").resolve()):
        parser.error("output must be outside the project and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    receipt_path = output / "receipt-leaf-families.json"
    log_path = output / "leaf_family_oracle.log"
    log_path.write_text("", encoding="utf-8")
    receipt: dict = {"schema_version": 1, "suite": "exact-leaf-family-batch",
                     "status": "failed", "passed": False, "log_path": str(log_path)}
    publish(receipt_path, receipt)
    try:
        inventory_bytes = INVENTORY.read_bytes()
        inventory = json.loads(inventory_bytes)
        if inventory["schema_version"] != 1:
            raise ValueError("unsupported exact-body inventory schema")
        if inventory["xex_sha256_from_catalog"] != EXPECTED_XEX_SHA256:
            raise ValueError("inventory XEX identity changed")
        for name, template in TEMPLATES.items():
            canonical = b"\n".join(template["lines"]) + b"\n"
            if sha(canonical) != template["body_sha256"]:
                raise ValueError(f"internal strict template changed: {name}")
        body_to_addresses = inventory["body_sha256_to_addresses"]
        templates_by_address: dict[str, str] = {}
        for name, template in TEMPLATES.items():
            for address in body_to_addresses[template["body_sha256"]]:
                if address in templates_by_address:
                    raise ValueError(f"address belongs to multiple templates: {address}")
                templates_by_address[address] = name
        expected_template_counts = {"blr": 855, "li_one_blr": 61,
                                    "li_one_mr_r8_blr": 26}
        if len(templates_by_address) != 942 or any(
            len(body_to_addresses[TEMPLATES[name]["body_sha256"]]) != count
            for name, count in expected_template_counts.items()
        ):
            raise ValueError("target leaf-family membership changed")
        catalog = Path(inventory["catalog_absolute_path"])
        if file_sha(catalog) != inventory["catalog_sha256"]:
            raise ValueError("read-only catalog hash changed")
        locations = catalog_locations(catalog, set(templates_by_address))
        ppc_root = args.ppc_root.resolve()
        bodies, original_sha, source_sha = source_bodies(
            ppc_root, locations, inventory, templates_by_address)
        xex_sha = file_sha(XEX)
        image_sha = file_sha(IMAGE)
        if xex_sha != EXPECTED_XEX_SHA256 or image_sha != EXPECTED_IMAGE_SHA256:
            raise ValueError("private XEX or unpacked image identity changed")
        image = IMAGE.read_bytes()
        entries = []
        for address in sorted(templates_by_address):
            template_name = templates_by_address[address]
            template = TEMPLATES[template_name]
            address_int = int(address, 16)
            expected = bytes.fromhex(template["image_bytes"])
            offset = address_int - IMAGE_BASE
            if offset < 0 or offset + len(expected) > len(image):
                raise ValueError(f"entry outside pinned unpacked image: {address}")
            actual = image[offset:offset + len(expected)]
            if actual != expected:
                raise ValueError(f"machine instructions disagree with body: {address}: {actual.hex()}")
            relative, line = locations[address]
            entries.append({"address": address, "template": template_name,
                            "generated_ppc_path": relative, "line": line})

        mapping = {
            "schema_version": 1,
            "kind": "exact_leaf_entry_to_shared_semantics",
            "inventory_sha256": sha(inventory_bytes),
            "catalog_sha256": inventory["catalog_sha256"],
            "xex_sha256": xex_sha,
            "image_sha256": image_sha,
            "semantic_families": {
                "PreserveR3": "lo::semantic::leaf::PreserveR3",
                "ReturnOne": "lo::semantic::leaf::ReturnOne",
            },
            "body_templates": {
                name: {"body_sha256": template["body_sha256"],
                       "image_bytes": template["image_bytes"],
                       "semantic": template["semantic"]}
                for name, template in TEMPLATES.items()
            },
            "source_sha256": source_sha,
            "entries": entries,
            "limits": [
                "942 distinct guest entry addresses share two readable C++ semantics; they are not 942 independent human implementations.",
                "Each entry requires its own original PPC and machine-word check plus differential cases.",
                "Unpacked image identity is independently pinned from XEX identity, but the unpacking provenance is not independently attested.",
                "No game-runtime replacement is enabled by this map.",
            ],
        }
        if args.write_map:
            publish(FAMILY_MAP, mapping)
        elif not FAMILY_MAP.is_file() or json.loads(FAMILY_MAP.read_text(encoding="utf-8")) != mapping:
            raise ValueError("tracked leaf family map missing or differs from verified sources")

        fixture = make_fixture(entries, bodies)
        fixture_path = output / "leaf_family_oracle_generated.cpp"
        fixture_path.write_bytes(fixture)
        executable = output / "leaf_family_oracle.exe"
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
                   str(fixture_path), str(ROOT / "LostOdysseyRecompSemantics/src/leaf_family.cpp"),
                   "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
        source_inputs = [
            "LostOdysseyRecompSemantics/include/lo_semantics/leaf_family.h",
            "LostOdysseyRecompSemantics/src/leaf_family.cpp",
            "LostOdysseyRecompSemantics/tests/leaf_family_oracle.cpp",
            "tools/ghidra/test_semantic_leaf_family.py",
            "tools/tests/run.py", "tools/setup_windows.bat",
            "LostOdysseyRecompLib/ppc/ppc_context.h",
            "LostOdysseyRecompLib/ppc/ppc_config.h",
        ]
        local_sources = {name: file_sha(ROOT / name) for name in source_inputs}
        receipt.update({
            "semantic_implementations": 2,
            "exact_body_templates": 3,
            "covered_entrypoints": len(entries),
            "cases_per_entry": CASES_PER_ENTRY,
            "total_cases": len(entries) * CASES_PER_ENTRY,
            "xex_path": str(XEX), "xex_sha256": xex_sha,
            "image_path": str(IMAGE), "image_sha256": image_sha,
            "image_base": f"0x{IMAGE_BASE:08X}",
            "catalog_path": str(catalog), "catalog_sha256": inventory["catalog_sha256"],
            "inventory_path": str(INVENTORY), "inventory_sha256": sha(inventory_bytes),
            "family_map_path": str(FAMILY_MAP), "family_map_sha256": file_sha(FAMILY_MAP),
            "generated_ppc_root": str(ppc_root),
            "generated_ppc_source_sha256": source_sha,
            "local_source_sha256": local_sources,
            "generated_fixture_path": str(fixture_path),
            "generated_fixture_sha256": sha(fixture),
            "compiler": compiler, "compile_command": command,
            "entries": [
                {**entry,
                 "original_sha256": original_sha[entry["address"]],
                 "body_sha256": TEMPLATES[entry["template"]]["body_sha256"],
                 "semantic": TEMPLATES[entry["template"]]["semantic"],
                 "image_bytes": TEMPLATES[entry["template"]]["image_bytes"],
                 "cases": CASES_PER_ENTRY}
                for entry in entries
            ],
            "boundaries": [
                "Original generated PPC and full PPCContext bytes, including initialized padding, are compared per entry.",
                "All 4096 ordinary mapped memory bytes are compared per case; none of the three strict templates accesses memory.",
                "Each original body is hash-pinned, catalog-located and checked against all expected instructions; actual unpacked-image instruction words are checked per entry.",
                "XEX and image are separately hash-pinned; decryption/unpacking provenance remains unproven.",
                "This is no-fault local equivalence, not game runtime, MMIO, timing, or general ABI replacement evidence.",
            ],
        })
        publish(receipt_path, receipt)
        with log_path.open("a", encoding="utf-8") as log:
            log.write("+ " + subprocess.list2cmdline(command) + "\n")
            started = time.perf_counter()
            built = subprocess.run(command, cwd=ROOT, env=environment,
                                   text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=300)
            compile_seconds = time.perf_counter() - started
            log.write(built.stdout)
            print(built.stdout, end="", flush=True)
            if built.returncode:
                raise subprocess.CalledProcessError(built.returncode, command)
            started = time.perf_counter()
            run = subprocess.run([str(executable)], cwd=ROOT, text=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 timeout=120)
            execute_seconds = time.perf_counter() - started
            log.write("+ " + str(executable) + "\n" + run.stdout)
            print(run.stdout, end="", flush=True)
            if run.returncode:
                raise subprocess.CalledProcessError(run.returncode, [str(executable)])
        expected = f"PASS leaf-family 942 entries 4710 cases 855 preserve 87 one"
        if expected not in run.stdout.splitlines():
            raise RuntimeError("batch oracle did not report exact per-entry completion")
        receipt.update(status="passed", passed=True,
                       compiler_seconds=round(compile_seconds, 3),
                       execution_seconds=round(execute_seconds, 3),
                       executable_path=str(executable), executable_sha256=file_sha(executable))
    except Exception as error:
        receipt["reason"] = str(error)
        raise
    finally:
        publish(receipt_path, receipt)
    print(f"receipt: {receipt_path}")


if __name__ == "__main__":
    main()
