#!/usr/bin/env python3
"""Join the address catalog, recovery manifest and current differential receipts.

The address union is a work inventory, not a count of proven logical functions.
Without a matching receipt, a manifest claim does not count as validation.
"""

from __future__ import annotations

import argparse
from contextlib import closing
import csv
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import sys


ROOT = Path(__file__).resolve().parents[2]
STAGES = ("readable_implementation", "differential_validation", "runtime_integration",
          "scene_validation", "complete_semantics")


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


class SourceSnapshot:
    """Reuse hashes and parsed bodies only within one validation invocation."""

    def __init__(self) -> None:
        self.hashes: dict[Path, str] = {}
        self.identities: dict[Path, tuple[int, int]] = {}
        self.function_bodies: dict[Path, dict[str, str]] = {}

    def read(self, path: Path) -> bytes:
        path = path.resolve()
        before = path.stat()
        data = path.read_bytes()
        after = path.stat()
        identity = (after.st_size, after.st_mtime_ns)
        if (before.st_size, before.st_mtime_ns) != identity:
            raise ValueError(f"source changed while reading: {path}")
        actual = hashlib.sha256(data).hexdigest()
        if path in self.hashes and self.hashes[path] != actual:
            raise ValueError(f"source changed during validation: {path}")
        self.hashes[path] = actual
        self.identities[path] = identity
        return data

    def digest(self, path: Path) -> str:
        path = path.resolve()
        if path not in self.hashes:
            self.read(path)
        return self.hashes[path]

    def body_digest(self, path: Path, addr: str) -> str | None:
        path = path.resolve()
        if path not in self.function_bodies:
            bodies = {}
            pattern = rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{.*?\r?\n\}"
            for match in re.finditer(pattern, self.read(path), re.S):
                found = match.group(1).decode('ascii')
                if found in bodies:
                    raise ValueError(f"duplicate original function body: {found} in {path}")
                bodies[found] = hashlib.sha256(match.group()).hexdigest()
            self.function_bodies[path] = bodies
        return self.function_bodies[path].get(addr)

    def check_unchanged(self) -> None:
        for path, identity in self.identities.items():
            current = path.stat()
            if (current.st_size, current.st_mtime_ns) != identity:
                raise ValueError(f"source changed during validation: {path}")


def address(value: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9A-F]{8}", value):
        raise ValueError(f"expected uppercase eight-digit address: {value!r}")
    return value


def repo_file(value: str) -> Path:
    path = (ROOT / value).resolve()
    if not path.is_relative_to(ROOT) or not path.is_file():
        raise ValueError(f"missing repository source or path outside repository: {value}")
    return path


def check_receipt(path: Path, functions: dict, generated: dict, xex_sha: str,
                  snapshot: SourceSnapshot) -> tuple[str, dict]:
    receipt = json.loads(path.read_text(encoding="utf-8"))
    addr = address(receipt.get("function_address"))
    if addr not in functions:
        raise ValueError(f"{path}: function is absent from recovery manifest")
    if receipt.get("passed") is not True or receipt.get("status") != "passed":
        raise ValueError(f"{path}: differential test did not pass")
    if receipt.get("xex_sha256") != xex_sha:
        raise ValueError(f"{path}: XEX identity mismatch")
    symbol = f"sub_{addr}"
    if receipt.get("original_function") != symbol:
        raise ValueError(f"{path}: original function does not match receipt address")
    reference = Path(receipt["generated_ppc_path"]).resolve()
    if addr not in generated or reference != repo_file(generated[addr].rsplit(":", 1)[0]):
        raise ValueError(f"{path}: original function source does not match catalog")
    body_hash = snapshot.body_digest(reference, addr)
    if body_hash is None or body_hash != receipt.get("original_function_sha256"):
        raise ValueError(f"{path}: original function body identity mismatch")
    if type(receipt.get("cases")) is not int or receipt["cases"] <= 0:
        raise ValueError(f"{path}: missing positive case count")
    hashes = receipt.get("source_sha256", {})
    required = {functions[addr]["source"], functions[addr]["header"]}
    if not required.issubset(hashes):
        raise ValueError(f"{path}: recovered source/header hashes are missing")
    for name, expected in hashes.items():
        if snapshot.digest(repo_file(name)) != expected:
            raise ValueError(f"{path}: stale receipt; source changed: {name}")
    # Check the private reference and generated fixture as well as public sources.
    for prefix in ("xex", "generated_ppc", "generated_fixture"):
        source = Path(receipt[f"{prefix}_path"])
        if snapshot.digest(source) != receipt[f"{prefix}_sha256"]:
            raise ValueError(f"{path}: stale receipt; {prefix} changed")
    return addr, receipt


def run(args: argparse.Namespace) -> None:
    snapshot = SourceSnapshot()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        raise ValueError("unsupported recovery manifest schema")
    xex_sha = manifest.get("xex_sha256")
    if not isinstance(xex_sha, str) or not re.fullmatch(r"[0-9a-f]{64}", xex_sha):
        raise ValueError("manifest needs a lowercase XEX SHA-256")
    functions = {}
    for function in manifest["functions"]:
        addr = address(function["address"])
        if addr in functions:
            raise ValueError(f"duplicate recovered function {addr}")
        for field in ("source", "header"):
            repo_file(function[field])
        stages = function["stages"]
        if set(stages) != set(STAGES) or any(type(value) is not bool for value in stages.values()):
            raise ValueError(f"{addr}: expected explicit boolean recovery stages")
        if stages["complete_semantics"] and (
                not all(stages.values()) or function["open_obligations"] or
                any(item["status"] != "recovered" for item in function["dependencies"])):
            raise ValueError(f"{addr}: complete semantics conflicts with open obligations")
        functions[addr] = function

    with closing(sqlite3.connect(args.catalog.resolve().as_uri() + "?mode=ro", uri=True)) as db:
        metadata = dict(db.execute("SELECT key,value FROM metadata"))
        if metadata.get("xex_sha256") != xex_sha:
            raise ValueError("catalog and recovery manifest XEX identities differ")
        names = dict(db.execute("SELECT address,name FROM functions"))
        generated = {}
        for addr, path, line in db.execute(
                "SELECT address,path,line FROM evidence WHERE kind='generated_ppc' ORDER BY id"):
            generated.setdefault(addr, f"{path}:{line}")
    addresses = sorted(names.keys() | generated.keys())
    if functions.keys() - set(addresses):
        raise ValueError("recovered function is absent from both address inventories")
    receipts = {}
    for path in args.receipt:
        addr, receipt = check_receipt(path, functions, generated, xex_sha, snapshot)
        if addr in receipts:
            raise ValueError(f"multiple receipts supplied for {addr}")
        receipts[addr] = receipt
    for addr, function in functions.items():
        if function["stages"]["complete_semantics"]:
            for dependency in function["dependencies"]:
                target = dependency["address"]
                if target not in functions or not functions[target]["stages"]["complete_semantics"]:
                    raise ValueError(f"{addr}: complete semantics requires a complete dependency record for {target}")

    def current_complete(addr: str) -> bool:
        pending, seen = [addr], set()
        while pending:
            target = pending.pop()
            if target in seen:
                continue
            seen.add(target)
            if target not in receipts or not functions[target]["stages"]["complete_semantics"]:
                return False
            pending.extend(item["address"] for item in functions[target]["dependencies"])
        return True

    counts = {stage: sum(f["stages"][stage] for f in functions.values()) for stage in STAGES}
    counts["differential_validation"] = len(receipts)
    # A completion declaration cannot be presented as current without its receipt.
    counts["complete_semantics"] = sum(current_complete(addr) for addr in functions)
    lines = ["# Semantic recovery progress", "",
             f"XEX SHA-256: `{xex_sha}`", "",
             f"Inventory: **{len(addresses):,} candidate entry addresses** "
             f"({len(names):,} Ghidra entries; {len(generated):,} generated PPC entries). "
             "Overlapping/interior entries and missing function boundaries remain unresolved; "
             "this is not a count of distinct logical functions or a percentage of game behavior.", "",
             "| Stage | Count | Evidence |", "| --- | ---: | --- |"]
    for stage in STAGES:
        evidence = "Current source-hash-checked receipts" if stage == "differential_validation" else "Recovery manifest"
        lines.append(f"| {stage} | {counts[stage]} | {evidence} |")
    lines.extend(["", "A missing receipt counts as unverified even if the manifest records a previous pass. "
                  "Runtime and scene stages are separate declarations requiring their own acceptance evidence.",
                  "", "## Recovered implementations", ""])
    for addr, function in functions.items():
        receipt = receipts.get(addr)
        lines.extend([f"### {addr} — {function['name']}", "", f"Source: `{function['source']}`", "",
                      f"Differential cases: **{receipt['cases']}**." if receipt else "Differential evidence: not supplied.", ""])
        if receipt:
            lines.extend(f"- {boundary}" for boundary in receipt.get("boundaries", []))
            lines.append("")
        lines.append("Dependencies:")
        lines.append("")
        lines.extend(f"- `{d['address']}` `{d['boundary']}`: {d['status']}" for d in function["dependencies"])
        lines.extend(["", "Remaining obligations:", ""])
        lines.extend(f"- {item}" for item in function["open_obligations"])
        lines.append("")
    lines.extend(["## Full recovery order", "",
                  "Resolve candidate function boundaries and calling conventions; recover shared types and "
                  "leaf contracts; compare implementations against original PPC; integrate one function "
                  "at a time behind a reversible adapter; validate real scenes, lifetime and concurrency "
                  "before retiring the corresponding generated implementation.", "",
                  "The address TSV is a work queue. Unrecovered addresses have no generated semantic stubs.", ""])
    # Validate all inputs before replacing reports so a failed run preserves prior evidence.
    snapshot.check_unchanged()
    args.output.mkdir(parents=True, exist_ok=True)
    report = args.output / "report.md"
    temporary = report.with_suffix(".md.tmp")
    temporary.write_text("\n".join(lines), encoding="utf-8")
    temporary.replace(report)
    table = args.output / "addresses.tsv"
    temporary = table.with_suffix(".tsv.tmp")
    with temporary.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t")
        writer.writerow(["address", "ghidra_name", "generated_source", "semantic_name", "semantic_source", *STAGES])
        for addr in addresses:
            function = functions.get(addr, {})
            flags = function.get("stages", {})
            values = [int(addr in receipts) if stage == "differential_validation" else
                      int(current_complete(addr)) if stage == "complete_semantics" and function else
                      int(bool(flags.get(stage)))
                      for stage in STAGES]
            writer.writerow([addr, names.get(addr, ""), generated.get(addr, ""),
                             function.get("name", ""), function.get("source", ""), *values])
    temporary.replace(table)
    print(f"{report}: {len(addresses)} candidate addresses, "
          f"{counts['readable_implementation']} readable, {len(receipts)} differential, "
          f"{counts['runtime_integration']} runtime, {counts['complete_semantics']} complete")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=ROOT / "LostOdysseyRecompSemantics/recovery.json")
    parser.add_argument("--receipt", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    try:
        run(parser.parse_args())
    except (OSError, ValueError, KeyError, TypeError, sqlite3.Error) as error:
        print(f"semantic progress error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
