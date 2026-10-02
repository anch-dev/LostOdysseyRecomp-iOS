#!/usr/bin/env python3
"""Build and inspect a source-linked, version-checked Ghidra analysis catalog.

The catalog is an evidence index. A source hook, a document mention, and an old
decompiler text file are deliberately not treated as verified game semantics.
"""

from __future__ import annotations

import argparse
from bisect import bisect_right
from contextlib import closing
import hashlib
import json
import os
import re
import sqlite3
import sys
import tempfile
from pathlib import Path


ADDRESS = re.compile(r"^(?:0x)?([0-9a-fA-F]{8})$")
GENERATED = re.compile(r"^\s*PPC_FUNC_IMPL\s*\(\s*__imp__sub_([0-9a-fA-F]{8})\s*\)")
HOOK = re.compile(r"\bPPC_FUNC\s*\(\s*sub_([0-9a-fA-F]{8})\s*\)\s*\{", re.S)
DOC_ADDRESS = re.compile(r"(?<![0-9A-Za-z_])(?:0x|sub_)?(8[0-9a-fA-F]{7})(?![0-9A-Za-z_])", re.I)
DECOMP_WARNING = re.compile(r"\bWARNING\b|\bhalt_baddata\s*\(", re.I)
SEVERE_DECOMP_WARNING = re.compile(r"bad instruction|truncating control flow|\bhalt_baddata\s*\(", re.I)
OLD_REQUEST = re.compile(r"^//\s*Requested(?:\s+addr:)?\s*(?:0x)?([0-9a-fA-F]{8})\b", re.I)
OLD_ENTRY = re.compile(r"^//\s*Entry(?:\s+addr:)?\s*(?:0x)?([0-9a-fA-F]{8})\b", re.I)
OLD_REFERENCE = re.compile(r"^//\s*Reference(?:\s+addr\s+caller)?\s*(?:0x)?([0-9a-fA-F]{8})\b(?:\s+(\S+))?", re.I)
CPP_TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
MAX_LEGACY_BYTES = 4 * 1024 * 1024


def address(value: object, field: str = "address") -> str:
    if not isinstance(value, str) or not (match := ADDRESS.fullmatch(value)):
        raise ValueError(f"invalid {field}: {value!r}; expected eight hex digits")
    return match.group(1).upper()


def source_path(repo: Path, path: Path) -> str:
    """Use portable paths for repository files and absolute paths for external inputs."""
    path = path.resolve()
    try:
        return path.relative_to(repo).as_posix()
    except ValueError:
        return str(path)


def input_path(repo: Path, value: str) -> Path:
    path = Path(value)
    return (path if path.is_absolute() else repo / path).resolve()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def jsonl(path: Path):
    if not path.is_file():
        raise ValueError(f"missing JSONL input: {path}")
    with path.open("r", encoding="utf-8") as stream:
        for line_number, raw in enumerate(stream, 1):
            if not raw.strip():
                raise ValueError(f"{path}:{line_number}: blank JSONL record")
            try:
                record = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_number}: invalid JSON: {exc}") from exc
            if not isinstance(record, dict):
                raise ValueError(f"{path}:{line_number}: expected JSON object")
            yield line_number, record


def checked_program(record: dict, xex_sha: str, path: Path) -> None:
    if record.get("kind") != "program":
        raise ValueError(f"{path}: first record must be kind=program")
    digest = record.get("executable_sha256")
    if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", digest):
        raise ValueError(f"{path}: program executable_sha256 is missing or invalid")
    if digest.lower() != xex_sha:
        raise ValueError(f"{path}: XEX SHA256 mismatch: inventory {digest.lower()}, input {xex_sha}")


SCHEMA = """
CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE functions (address TEXT PRIMARY KEY, name TEXT NOT NULL, signature TEXT NOT NULL,
                       is_thunk INTEGER NOT NULL, is_external INTEGER NOT NULL);
CREATE TABLE body_ranges (function_address TEXT NOT NULL, start INTEGER NOT NULL, end INTEGER NOT NULL,
                          CHECK (start <= end));
CREATE INDEX body_ranges_lookup ON body_ranges(start, end);
CREATE TABLE static_calls (caller TEXT NOT NULL, callee TEXT NOT NULL, site TEXT NOT NULL,
                           reference_type TEXT NOT NULL, computed INTEGER NOT NULL);
CREATE INDEX calls_caller ON static_calls(caller);
CREATE INDEX calls_callee ON static_calls(callee);
CREATE TABLE evidence (id INTEGER PRIMARY KEY, address TEXT NOT NULL, kind TEXT NOT NULL,
                       path TEXT NOT NULL, line INTEGER NOT NULL, detail TEXT NOT NULL DEFAULT '',
                       requested TEXT, related_address TEXT);
CREATE INDEX evidence_address ON evidence(address);
CREATE INDEX evidence_requested ON evidence(requested);
CREATE TABLE annotations (address TEXT PRIMARY KEY, label TEXT NOT NULL, category TEXT NOT NULL,
                          confidence TEXT NOT NULL);
CREATE TABLE annotation_evidence (address TEXT NOT NULL, path TEXT NOT NULL, line INTEGER NOT NULL,
                                  note TEXT NOT NULL);
CREATE INDEX annotation_evidence_address ON annotation_evidence(address);
CREATE TABLE decomp (id INTEGER PRIMARY KEY, requested TEXT NOT NULL, address TEXT, name TEXT,
                     status TEXT NOT NULL, error TEXT, path TEXT NOT NULL, line INTEGER NOT NULL);
CREATE INDEX decomp_address ON decomp(address);
CREATE INDEX decomp_requested ON decomp(requested);
"""


def meta(db: sqlite3.Connection, key: str, value: object) -> None:
    db.execute("INSERT INTO metadata VALUES (?, ?)", (key, str(value)))


def evidence(db: sqlite3.Connection, addr: str, kind: str, path: str, line: int,
             detail: str = "", requested: str | None = None, related: str | None = None) -> None:
    db.execute("INSERT INTO evidence(address,kind,path,line,detail,requested,related_address) "
               "VALUES (?,?,?,?,?,?,?)", (addr, kind, path, line, detail, requested, related))


def load_inventory(db: sqlite3.Connection, repo: Path, path: Path, xex_sha: str) -> None:
    records = jsonl(path)
    try:
        _, program = next(records)
    except StopIteration as exc:
        raise ValueError(f"{path}: empty inventory") from exc
    checked_program(program, xex_sha, path)
    for field in ("name", "executable_path", "image_base", "language", "compiler_spec", "function_count"):
        if field not in program:
            raise ValueError(f"{path}: program missing {field}")
    expected = program["function_count"]
    if not isinstance(expected, int) or expected < 0:
        raise ValueError(f"{path}: invalid function_count")
    function_count = call_count = 0
    complete = False
    for line, record in records:
        kind = record.get("kind")
        if complete:
            raise ValueError(f"{path}:{line}: record after complete marker")
        if kind == "function":
            addr = address(record.get("address"))
            name = record.get("name")
            signature = record.get("signature")
            if not isinstance(name, str) or not isinstance(signature, str):
                raise ValueError(f"{path}:{line}: function name/signature must be strings")
            ranges = record.get("body_ranges")
            if not isinstance(ranges, list):
                raise ValueError(f"{path}:{line}: function missing body_ranges")
            db.execute("INSERT INTO functions VALUES (?,?,?,?,?)",
                       (addr, name, signature, int(bool(record.get("is_thunk"))),
                        int(bool(record.get("is_external")))))
            for pair in ranges:
                if not isinstance(pair, list) or len(pair) != 2:
                    raise ValueError(f"{path}:{line}: invalid body range {pair!r}")
                start, end = (int(address(item, "body range"), 16) for item in pair)
                db.execute("INSERT INTO body_ranges VALUES (?,?,?)", (addr, start, end))
            function_count += 1
        elif kind == "call":
            caller = address(record.get("caller"), "caller")
            callee = address(record.get("callee"), "callee")
            site = address(record.get("site"), "site")
            reference_type = record.get("reference_type")
            if not isinstance(reference_type, str):
                raise ValueError(f"{path}:{line}: invalid reference_type")
            db.execute("INSERT INTO static_calls VALUES (?,?,?,?,?)",
                       (caller, callee, site, reference_type, int(bool(record.get("computed")))))
            call_count += 1
        elif kind == "complete":
            if record.get("function_count") != function_count or record.get("call_count") != call_count:
                raise ValueError(f"{path}:{line}: completion counts do not match records")
            if expected != function_count:
                raise ValueError(f"{path}:{line}: program function_count does not match records")
            complete = True
        else:
            raise ValueError(f"{path}:{line}: unexpected kind {kind!r}")
    if not complete:
        raise ValueError(f"{path}: incomplete inventory (missing complete marker)")
    meta(db, "inventory_path", source_path(repo, path))
    meta(db, "program_name", program["name"])
    meta(db, "program_executable_path", program["executable_path"])
    meta(db, "image_base", program["image_base"])
    meta(db, "language", program["language"])
    meta(db, "compiler_spec", program["compiler_spec"])
    meta(db, "inventory_functions", function_count)
    meta(db, "inventory_calls", call_count)


def load_decomp(db: sqlite3.Connection, repo: Path, directory: Path, xex_sha: str) -> None:
    manifest = directory / "manifest.jsonl"
    records = jsonl(manifest)
    try:
        _, program = next(records)
    except StopIteration as exc:
        raise ValueError(f"{manifest}: empty manifest") from exc
    checked_program(program, xex_sha, manifest)
    counts = {"ok": 0, "missing": 0, "failed": 0}
    complete = False
    for line, record in records:
        kind = record.get("kind")
        if complete:
            raise ValueError(f"{manifest}:{line}: record after complete marker")
        if kind == "decomp":
            requested = address(record.get("requested"), "requested")
            actual = record.get("address")
            actual = address(actual) if actual is not None else None
            status = record.get("status")
            if status not in counts:
                raise ValueError(f"{manifest}:{line}: invalid decomp status {status!r}")
            filename = record.get("filename")
            file_sha = record.get("sha256")
            error = record.get("error")
            name = record.get("name")
            if name is not None and not isinstance(name, str):
                raise ValueError(f"{manifest}:{line}: invalid decomp name")
            if error is not None and not isinstance(error, str):
                raise ValueError(f"{manifest}:{line}: invalid decomp error")
            if status == "ok":
                if actual is None or not isinstance(filename, str) or not filename:
                    raise ValueError(f"{manifest}:{line}: successful decomp needs address and filename")
                if not isinstance(file_sha, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", file_sha):
                    raise ValueError(f"{manifest}:{line}: successful decomp needs file sha256")
                source = (directory / filename).resolve()
                if source.parent != directory.resolve() or not source.is_file() or source.stat().st_size == 0:
                    raise ValueError(f"{manifest}:{line}: missing/empty decomp file {filename!r}")
                if sha256_file(source) != file_sha.lower():
                    raise ValueError(f"{manifest}:{line}: decomp file SHA256 mismatch: {filename}")
                matches = db.execute(
                    "SELECT 1 FROM functions f LEFT JOIN body_ranges r ON r.function_address=f.address "
                    "WHERE f.address=? AND (f.address=? OR ? BETWEEN r.start AND r.end) LIMIT 1",
                    (actual, requested, int(requested, 16))).fetchone()
                if matches is None:
                    raise ValueError(f"{manifest}:{line}: decomp {requested} -> {actual} "
                                     "does not match an inventory function/body range")
                path = source_path(repo, source)
                evidence(db, actual, "fresh_decomp", path, 1, name or "", requested)
                with source.open("r", encoding="utf-8", errors="replace") as stream:
                    for source_line, raw in enumerate(stream, 1):
                        if DECOMP_WARNING.search(raw):
                            evidence(db, actual, "fresh_decomp_warning", path, source_line,
                                     raw.strip(), requested)
            else:
                if filename is not None or file_sha is not None:
                    raise ValueError(f"{manifest}:{line}: non-ok decomp must not have filename/sha256")
                path = source_path(repo, manifest)
                if actual is not None:
                    evidence(db, actual, f"decomp_{status}", path, line, error or "", requested)
            db.execute("INSERT INTO decomp(requested,address,name,status,error,path,line) VALUES (?,?,?,?,?,?,?)",
                       (requested, actual, name, status, error, path, 1 if status == "ok" else line))
            counts[status] += 1
        elif kind == "complete":
            if (record.get("request_count") != sum(counts.values()) or
                any(record.get(f"{key}_count") != value for key, value in counts.items())):
                raise ValueError(f"{manifest}:{line}: completion counts do not match records")
            complete = True
        else:
            raise ValueError(f"{manifest}:{line}: unexpected kind {kind!r}")
    if not complete:
        raise ValueError(f"{manifest}: incomplete manifest (missing complete marker)")
    manifest_index = db.execute("SELECT count(*) FROM metadata WHERE key LIKE 'decomp_manifest_%'").fetchone()[0] + 1
    meta(db, f"decomp_manifest_{manifest_index}", source_path(repo, manifest))


def mask_cpp(source: str) -> str:
    return CPP_TOKEN.sub(lambda match: re.sub(r"[^\n]", " ", match.group()), source)


def scan_sources(db: sqlite3.Connection, repo: Path) -> None:
    generated = repo / "LostOdysseyRecompLib" / "ppc"
    for path in sorted(generated.glob("ppc_recomp.*.cpp")):
        with path.open("r", encoding="utf-8", errors="replace") as stream:
            for line, raw in enumerate(stream, 1):
                if match := GENERATED.match(raw):
                    evidence(db, match.group(1).upper(), "generated_ppc", source_path(repo, path), line)
    runtime = repo / "LostOdysseyRecomp"
    for path in sorted((*runtime.rglob("*.cpp"), *runtime.rglob("*.h"))):
        source = path.read_text(encoding="utf-8", errors="replace")
        clean = mask_cpp(source)
        for match in HOOK.finditer(clean):
            line = clean.count("\n", 0, match.start()) + 1
            evidence(db, match.group(1).upper(), "source_hook", source_path(repo, path), line,
                     "source definition; build activation unverified")
    docs = repo / "docs"
    for path in sorted(docs.rglob("*.md")):
        with path.open("r", encoding="utf-8", errors="replace") as stream:
            for line, raw in enumerate(stream, 1):
                for addr in sorted({match.group(1).upper() for match in DOC_ADDRESS.finditer(raw)}):
                    evidence(db, addr, "document_mention", source_path(repo, path), line,
                             "address mention; semantics unverified")


def scan_legacy(db: sqlite3.Connection, repo: Path) -> None:
    out = repo / "out"
    if not out.is_dir():
        return
    for path in sorted(out.iterdir()):
        if not path.is_file() or path.suffix.lower() not in (".txt", ".c"):
            continue
        if path.stat().st_size >= MAX_LEGACY_BYTES:
            continue
        requested = actual = None
        with path.open("r", encoding="utf-8", errors="replace") as stream:
            for line, raw in enumerate(stream, 1):
                raw = raw.strip()
                if match := OLD_REQUEST.match(raw):
                    requested = match.group(1).upper()
                    actual = None
                    evidence(db, requested, "legacy_request", source_path(repo, path), line,
                             "requested address; old output is unverified", requested)
                elif match := OLD_ENTRY.match(raw):
                    actual = match.group(1).upper()
                    evidence(db, actual, "legacy_unverified_source", source_path(repo, path), line,
                             "entry address; old output is unverified", requested)
                elif match := OLD_REFERENCE.match(raw):
                    site = match.group(1).upper()
                    caller = match.group(2) or ""
                    if actual:
                        evidence(db, actual, "legacy_reference", source_path(repo, path), line,
                                 f"reference site {site}; caller {caller}; unverified", requested, site)


def load_annotations(db: sqlite3.Connection, repo: Path, path: Path, xex_sha: str) -> None:
    if not path.is_file():
        raise ValueError(f"missing annotations: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or data.get("schema_version") != 1 or not isinstance(data.get("functions"), list):
        raise ValueError(f"{path}: expected schema_version=1 and functions array")
    digest = data.get("source_sha256")
    if digest is not None and (not isinstance(digest, str) or digest.lower() != xex_sha):
        raise ValueError(f"{path}: annotation source_sha256 differs from XEX")
    for index, item in enumerate(data["functions"]):
        if not isinstance(item, dict):
            raise ValueError(f"{path}: invalid function annotation #{index}")
        addr = address(item.get("address"))
        fields = (item.get("label"), item.get("category"), item.get("confidence"))
        if any(not isinstance(value, str) or not value.strip() for value in fields):
            raise ValueError(f"{path}: annotation {addr} needs label/category/confidence")
        citations = item.get("evidence")
        if not isinstance(citations, list) or not citations:
            raise ValueError(f"{path}: annotation {addr} needs evidence")
        db.execute("INSERT INTO annotations VALUES (?,?,?,?)", (addr, *fields))
        for cite in citations:
            if not isinstance(cite, dict) or not isinstance(cite.get("path"), str):
                raise ValueError(f"{path}: annotation {addr} has invalid evidence path")
            line = cite.get("line")
            note = cite.get("note")
            if not isinstance(line, int) or line < 1 or not isinstance(note, str):
                raise ValueError(f"{path}: annotation {addr} has invalid evidence line/note")
            source = input_path(repo, cite["path"])
            if not source.is_file():
                raise ValueError(f"{path}: annotation {addr} evidence missing: {source}")
            display = source_path(repo, source)
            db.execute("INSERT INTO annotation_evidence VALUES (?,?,?,?)", (addr, display, line, note))
            evidence(db, addr, "annotation_citation", display, line, note)
    meta(db, "annotations_path", source_path(repo, path))


def build(args: argparse.Namespace) -> None:
    repo = Path(args.repo).resolve()
    if not repo.is_dir():
        raise ValueError(f"repository missing: {repo}")
    xex = input_path(repo, args.xex)
    if not xex.is_file():
        raise ValueError(f"XEX missing: {xex}")
    xex_sha = sha256_file(xex)
    target = input_path(repo, args.output)
    out = (repo / "out").resolve()
    if target == out or out not in target.parents:
        raise ValueError(f"catalog output must be inside ignored {out}")
    target.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".catalog-", suffix=".sqlite", dir=target.parent)
    os.close(fd)
    try:
        with closing(sqlite3.connect(temporary)) as db:
            db.executescript(SCHEMA)
            meta(db, "schema_version", 1)
            meta(db, "repo", repo)
            meta(db, "xex_path", source_path(repo, xex))
            meta(db, "xex_sha256", xex_sha)
            load_inventory(db, repo, input_path(repo, args.inventory), xex_sha)
            scan_sources(db, repo)
            scan_legacy(db, repo)
            if args.annotations:
                load_annotations(db, repo, input_path(repo, args.annotations), xex_sha)
            for directory in args.decomp_dir:
                load_decomp(db, repo, input_path(repo, directory), xex_sha)
            db.commit()
            if db.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
                raise ValueError("catalog SQLite integrity check failed")
        os.replace(temporary, target)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    print(f"built {target} (XEX SHA256 {xex_sha})")


def connect(path: str) -> sqlite3.Connection:
    if not Path(path).is_file():
        raise ValueError(f"catalog missing: {path}")
    db = sqlite3.connect(f"file:{Path(path).resolve().as_posix()}?mode=ro", uri=True)
    db.row_factory = sqlite3.Row
    return db


def rowdicts(rows) -> list[dict]:
    return [dict(row) for row in rows]


def show_record(db: sqlite3.Connection, addr: str) -> dict:
    value = int(addr, 16)
    functions = rowdicts(db.execute(
        "SELECT DISTINCT f.* FROM functions f LEFT JOIN body_ranges r ON r.function_address=f.address "
        "WHERE f.address=? OR (? BETWEEN r.start AND r.end) ORDER BY f.address=? DESC, f.address",
        (addr, value, addr)))
    matched = [item["address"] for item in functions]
    addresses = set(matched) | {addr}
    legacy = rowdicts(db.execute(
        "SELECT address AS entry, path, line, detail FROM evidence "
        "WHERE kind='legacy_unverified_source' AND requested=? ORDER BY path,line", (addr,)))
    fresh = rowdicts(db.execute("SELECT * FROM decomp WHERE requested=? OR address=? ORDER BY requested", (addr, addr)))
    mapped_addresses = {item["entry"] for item in legacy}
    mapped_addresses.update(item["address"] for item in fresh if item["address"])
    mapped_functions = []
    for mapped in sorted(mapped_addresses - set(matched)):
        row = db.execute("SELECT * FROM functions WHERE address=?", (mapped,)).fetchone()
        if row:
            mapped_functions.append(dict(row))
    addresses.update(item["entry"] for item in legacy)
    addresses.update(item["address"] for item in fresh if item["address"])
    placeholder = ",".join("?" for _ in addresses)
    ev = rowdicts(db.execute(f"SELECT address,kind,path,line,detail,requested,related_address FROM evidence "
                             f"WHERE address IN ({placeholder}) OR requested=? ORDER BY kind,path,line",
                             (*sorted(addresses), addr)))
    annotations = rowdicts(db.execute(f"SELECT * FROM annotations WHERE address IN ({placeholder})",
                                      tuple(sorted(addresses))))
    for item in annotations:
        item["evidence"] = rowdicts(db.execute(
            "SELECT path,line,note FROM annotation_evidence WHERE address=? ORDER BY path,line", (item["address"],)))
    outgoing = incoming = []
    if matched:
        placeholder = ",".join("?" for _ in matched)
        outgoing = rowdicts(db.execute(
            f"SELECT * FROM static_calls WHERE caller IN ({placeholder}) ORDER BY caller,site", tuple(matched)))
        incoming = rowdicts(db.execute(
            f"SELECT * FROM static_calls WHERE callee IN ({placeholder}) ORDER BY callee,site", tuple(matched)))
    for item in functions:
        item["body_ranges"] = rowdicts(db.execute(
            "SELECT printf('%08X',start) AS start,printf('%08X',end) AS end "
            "FROM body_ranges WHERE function_address=? ORDER BY start", (item["address"],)))
        item["is_thunk"] = bool(item["is_thunk"])
        item["is_external"] = bool(item["is_external"])
    return {"requested_address": addr, "functions": functions,
            "mapped_functions": mapped_functions, "legacy_mappings": legacy,
            "decomp_mappings": fresh, "annotations": annotations, "evidence": ev,
            "outgoing_calls": outgoing, "incoming_calls": incoming,
            "call_graph_note": "Static references only; computed/virtual calls may be absent."}


def show(args: argparse.Namespace) -> None:
    with closing(connect(args.db)) as db:
        print(json.dumps(show_record(db, address(args.address)), ensure_ascii=False, indent=2))


def query(args: argparse.Namespace) -> None:
    term = args.term.strip()
    if not term:
        raise ValueError("empty search term")
    if args.limit < 1 or args.limit > 500:
        raise ValueError("limit must be between 1 and 500")
    with closing(connect(args.db)) as db:
        if ADDRESS.fullmatch(term):
            results = [show_record(db, address(term))]
        else:
            pattern = "%" + term.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + "%"
            rows = db.execute(
                "SELECT address FROM functions WHERE name LIKE ? ESCAPE '\\' OR signature LIKE ? ESCAPE '\\' "
                "UNION SELECT address FROM annotations WHERE label LIKE ? ESCAPE '\\' "
                "OR category LIKE ? ESCAPE '\\' "
                "UNION SELECT address FROM evidence WHERE path LIKE ? ESCAPE '\\' "
                "OR detail LIKE ? ESCAPE '\\' "
                "UNION SELECT address FROM annotation_evidence WHERE note LIKE ? ESCAPE '\\' "
                "ORDER BY address LIMIT ?", (pattern,) * 7 + (args.limit,)).fetchall()
            results = [show_record(db, row[0]) for row in rows]
        print(json.dumps(results, ensure_ascii=False, indent=2))


def report_text(db: sqlite3.Connection) -> str:
    metadata = dict(db.execute("SELECT key,value FROM metadata"))
    count = lambda sql: db.execute(sql).fetchone()[0]
    ghidra = count("SELECT count(*) FROM functions")
    entries = {row[0] for row in db.execute("SELECT address FROM functions")}
    merged_ranges: list[list[int]] = []
    for start, end in db.execute("SELECT start,end FROM body_ranges ORDER BY start,end"):
        if merged_ranges and start <= merged_ranges[-1][1] + 1:
            merged_ranges[-1][1] = max(end, merged_ranges[-1][1])
        else:
            merged_ranges.append([start, end])
    range_starts = [pair[0] for pair in merged_ranges]

    def in_body(addr: str) -> bool:
        value = int(addr, 16)
        index = bisect_right(range_starts, value) - 1
        return index >= 0 and value <= merged_ranges[index][1]

    generated_addresses = {row[0] for row in db.execute(
        "SELECT DISTINCT address FROM evidence WHERE kind='generated_ppc'")}
    generated_total = len(generated_addresses)
    generated_matching = len(generated_addresses & entries)
    generated_nonexact = generated_addresses - entries
    generated_inside = sum(in_body(addr) for addr in generated_nonexact)
    generated_outside = len(generated_nonexact) - generated_inside
    hook = count("SELECT count(DISTINCT address) FROM evidence WHERE kind='source_hook'")
    legacy = count("SELECT count(DISTINCT address) FROM evidence WHERE kind='legacy_unverified_source'")
    fresh = count("SELECT count(DISTINCT address) FROM decomp WHERE status='ok' AND address IS NOT NULL")
    warning_rows = db.execute("SELECT DISTINCT address,detail FROM evidence "
                              "WHERE kind='fresh_decomp_warning'").fetchall()
    warned_addresses = {row[0] for row in warning_rows}
    severe_addresses = {row[0] for row in warning_rows if SEVERE_DECOMP_WARNING.search(row[1])}
    other_warning_only = len(warned_addresses - severe_addresses)
    no_warning_markers = fresh - len(warned_addresses)
    failed = count("SELECT count(*) FROM decomp WHERE status='failed'")
    missing = count("SELECT count(*) FROM decomp WHERE status='missing'")
    annotated = count("SELECT count(*) FROM annotations")
    doc_candidates = {row[0] for row in db.execute(
        "SELECT DISTINCT e.address FROM evidence e WHERE e.kind='document_mention' "
        "AND NOT EXISTS (SELECT 1 FROM evidence x WHERE x.address=e.address "
        "AND x.kind<>'document_mention') "
        "AND NOT EXISTS (SELECT 1 FROM annotations a WHERE a.address=e.address)")}
    doc_without_entry = doc_candidates - entries
    docs_interior = sum(in_body(addr) for addr in doc_without_entry)
    docs_only = len(doc_without_entry) - docs_interior
    candidates = rowdicts(db.execute(
        "SELECT h.address, a.label, h.path, h.line, "
        "(SELECT count(*) FROM evidence d WHERE d.address=h.address AND d.kind='document_mention') AS docs "
        "FROM evidence h LEFT JOIN annotations a ON a.address=h.address "
        "WHERE h.kind='source_hook' AND NOT EXISTS "
        "(SELECT 1 FROM decomp x WHERE x.address=h.address AND x.status='ok') "
        "GROUP BY h.address ORDER BY (a.label IS NOT NULL) DESC, docs DESC, h.address LIMIT 20"))
    candidate_lines = "\n".join(
        f"- `{item['address']}` {item['label'] or '(unlabeled)'} — "
        f"`{item['path']}:{item['line']}`; document mentions: {item['docs']}"
        for item in candidates) or "- None"
    return f"""# Decomp analysis catalog

- Program: `{metadata['program_name']}`
- XEX SHA256: `{metadata['xex_sha256']}`
- Inventory: `{metadata['inventory_path']}`

| Evidence class | Count |
| --- | ---: |
| Ghidra inventory functions | {ghidra} |
| Resolved static call references | {metadata['inventory_calls']} |
| Generated PPC entries in local source | {generated_total} |
| Generated PPC entries matching Ghidra inventory addresses | {generated_matching} |
| Generated PPC addresses without exact Ghidra entry | {len(generated_nonexact)} |
| Of those, inside a Ghidra function body | {generated_inside} |
| Of those, outside Ghidra function bodies | {generated_outside} |
| Source hook definitions (build activation unverified) | {hook} |
| Legacy decomp entries (unverified source) | {legacy} |
| Fresh exports (pseudocode produced) | {fresh} |
| Fresh exports with bad instruction / truncated control flow markers | {len(severe_addresses)} |
| Fresh exports with other warnings only | {other_warning_only} |
| Fresh exports with no scanned warning markers | {no_warning_markers} |
| Fresh missing / failed requests | {missing} / {failed} |
| Document mentions inside Ghidra bodies, otherwise unlinked | {docs_interior} |
| Document-only addresses outside Ghidra bodies | {docs_only} |
| Evidence-backed annotations | {annotated} |

## Source hooks needing fresh decomp

These are investigation candidates ranked by existing annotation and document
links, not measured performance hotspots. Maximum 20 are shown.

{candidate_lines}

Evidence classes can overlap. A document mention or historical decompiler
output is not a semantic confirmation. Body ranges and
function entry points follow Ghidra's current analysis; recomp boundaries may
differ. Generated PPC entries are local source correlations; no separate XEX
manifest proves they came from this exact binary. Call edges are static
references only: computed and virtual calls can be missing. A source hook does
not prove it was compiled or active at runtime. A fresh `ok` status means the
decompiler produced a C file; even a file with no scanned warning marker is not
proof of complete or correct pseudocode. `show` and `query` expose warning text
with its source file and line number.
"""


def report(args: argparse.Namespace) -> None:
    with closing(connect(args.db)) as db:
        content = report_text(db)
        repo = Path(db.execute("SELECT value FROM metadata WHERE key='repo'").fetchone()[0])
    if args.output:
        output = Path(args.output)
        out = (repo / "out").resolve()
        if not output.is_absolute():
            from_cwd = (Path.cwd() / output).resolve()
            output = from_cwd if out in from_cwd.parents else Path(args.db).resolve().parent / output
        output = output.resolve()
        if output == out or out not in output.parents:
            raise ValueError(f"report output must be inside ignored {out}")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(content, encoding="utf-8")
        print(f"wrote {output}")
    else:
        print(content, end="")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("build", help="build an atomic, version-checked SQLite catalog")
    p.add_argument("--repo", required=True)
    p.add_argument("--inventory", required=True)
    p.add_argument("--xex", required=True)
    p.add_argument("--annotations")
    p.add_argument("--output", required=True)
    p.add_argument("--decomp-dir", action="append", default=[])
    p.set_defaults(action=build)
    p = sub.add_parser("query", help="search names, annotations, and evidence; JSON output")
    p.add_argument("--db", required=True)
    p.add_argument("term")
    p.add_argument("--limit", type=int, default=20)
    p.set_defaults(action=query)
    p = sub.add_parser("show", help="show an entry or containing body range; JSON output")
    p.add_argument("--db", required=True)
    p.add_argument("address")
    p.set_defaults(action=show)
    p = sub.add_parser("report", help="summarize evidence coverage as Markdown")
    p.add_argument("--db", required=True)
    p.add_argument("--output")
    p.set_defaults(action=report)
    args = parser.parse_args(argv)
    try:
        args.action(args)
    except (ValueError, OSError, sqlite3.Error) as exc:
        parser.exit(1, f"error: {exc}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
