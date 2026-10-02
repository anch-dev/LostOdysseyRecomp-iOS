"""Small, private-data-free fixtures for semantic recovery evidence gates."""

from __future__ import annotations

import argparse
from contextlib import closing, redirect_stdout
import csv
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "ghidra" / "semantic_progress.py"
SPEC = importlib.util.spec_from_file_location("semantic_progress", SCRIPT)
progress = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(progress)

FIRST = "82000000"
SECOND = "82000040"
GENERATED_ONLY = "82000080"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class SemanticProgressFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = (Path(self.temp.name) / "repo").resolve()
        self.repo.mkdir()
        self.source_name = "semantic/query.cpp"
        self.header_name = "semantic/query.h"
        self.ppc_name = "private/ppc_recomp.cpp"
        self.source = self.put(self.source_name, b"int recover_query() { return 1; }\n")
        self.header = self.put(self.header_name, b"int recover_query();\n")
        self.bodies = {
            FIRST: b"PPC_FUNC_IMPL(__imp__sub_82000000) {\n  return;\n}",
            SECOND: b"PPC_FUNC_IMPL(__imp__sub_82000040) {\n  return;\n}",
        }
        self.ppc = self.put(self.ppc_name,
                            self.bodies[FIRST] + b"\n\n" + self.bodies[SECOND] + b"\n")
        self.xex = self.put("private/original.xex", b"synthetic XEX identity only")
        self.fixture = self.put("private/compiled_fixture.cpp", b"synthetic generated fixture\n")
        self.catalog = self.repo / "catalog.sqlite"
        with closing(sqlite3.connect(self.catalog)) as db:
            db.executescript("""
                CREATE TABLE metadata (key TEXT, value TEXT);
                CREATE TABLE functions (address TEXT, name TEXT);
                CREATE TABLE evidence (id INTEGER PRIMARY KEY, address TEXT, path TEXT,
                                       line INTEGER, kind TEXT);
            """)
            db.execute("INSERT INTO metadata VALUES (?,?)", ("xex_sha256", sha(self.xex.read_bytes())))
            db.executemany("INSERT INTO functions VALUES (?,?)", [
                (FIRST, "GhidraFirst"), (SECOND, "GhidraSecond")])
            db.executemany("INSERT INTO evidence(address,path,line,kind) VALUES (?,?,?,?)", [
                (FIRST, self.ppc_name, 1, "generated_ppc"),
                (SECOND, self.ppc_name, 5, "generated_ppc"),
                (GENERATED_ONLY, self.ppc_name, 9, "generated_ppc"),
            ])
            db.commit()
        self.manifest = self.repo / "recovery.json"
        self.functions = [self.function(FIRST), self.function(SECOND)]
        self.write_manifest()
        self.receipt = self.repo / "first_receipt.json"
        self.write_receipt(self.receipt, FIRST)
        self.output = self.repo / "report"

    def put(self, name: str, data: bytes) -> Path:
        path = self.repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def function(self, addr: str) -> dict:
        return {
            "address": addr, "name": f"Semantic{addr}", "source": self.source_name,
            "header": self.header_name,
            "stages": {stage: stage in {"readable_implementation", "differential_validation"}
                       for stage in progress.STAGES},
            "dependencies": [], "open_obligations": ["pending runtime acceptance"],
        }

    def write_manifest(self) -> None:
        self.manifest.write_text(json.dumps({
            "schema_version": 1, "xex_sha256": sha(self.xex.read_bytes()),
            "functions": self.functions,
        }), encoding="utf-8")

    def write_receipt(self, path: Path, addr: str, **changes) -> dict:
        receipt = {
            "status": "passed", "passed": True, "cases": 7,
            "function_address": addr, "original_function": f"sub_{addr}",
            "original_function_sha256": sha(self.bodies[addr]),
            "source_sha256": {
                self.source_name: sha(self.source.read_bytes()),
                self.header_name: sha(self.header.read_bytes()),
            },
            "xex_path": str(self.xex), "xex_sha256": sha(self.xex.read_bytes()),
            "generated_ppc_path": str(self.ppc), "generated_ppc_sha256": sha(self.ppc.read_bytes()),
            "generated_fixture_path": str(self.fixture),
            "generated_fixture_sha256": sha(self.fixture.read_bytes()),
        }
        receipt.update(changes)
        path.write_text(json.dumps(receipt), encoding="utf-8")
        return receipt

    def run_progress(self, *receipts: Path) -> str:
        args = argparse.Namespace(catalog=self.catalog, manifest=self.manifest,
                                  receipt=list(receipts), output=self.output)
        output = io.StringIO()
        with patch.object(progress, "ROOT", self.repo), redirect_stdout(output):
            progress.run(args)
        return output.getvalue()

    def test_report_counts_and_address_tsv_use_current_receipts(self):
        summary = self.run_progress(self.receipt)
        report = (self.output / "report.md").read_text(encoding="utf-8")
        with (self.output / "addresses.tsv").open(encoding="utf-8", newline="") as stream:
            rows = {row["address"]: row for row in csv.DictReader(stream, delimiter="\t")}
        self.assertIn("3 candidate addresses, 2 readable, 1 differential", summary)
        self.assertIn("| readable_implementation | 2 |", report)
        self.assertIn("| differential_validation | 1 |", report)
        self.assertIn("| complete_semantics | 0 |", report)
        self.assertEqual(list(rows), [FIRST, SECOND, GENERATED_ONLY])
        self.assertEqual(rows[FIRST]["differential_validation"], "1")
        self.assertEqual(rows[SECOND]["differential_validation"], "0")
        self.assertEqual(rows[SECOND]["readable_implementation"], "1")
        self.assertEqual(rows[GENERATED_ONLY]["readable_implementation"], "0")
        self.assertEqual(rows[GENERATED_ONLY]["generated_source"], f"{self.ppc_name}:9")
        self.assertEqual(rows[GENERATED_ONLY]["semantic_source"], "")

    def test_shared_source_does_not_allow_wrong_original_identity(self):
        forged = self.repo / "second_receipt.json"
        for changes, expected in [
            ({"original_function": f"sub_{FIRST}"}, "original function"),
            ({"original_function_sha256": sha(self.bodies[FIRST])}, "body identity"),
        ]:
            with self.subTest(changes=changes):
                self.write_receipt(forged, SECOND, **changes)
                with self.assertRaisesRegex(ValueError, expected):
                    self.run_progress(self.receipt, forged)

    def test_stale_source_receipt_preserves_existing_report_and_tsv(self):
        self.run_progress(self.receipt)
        report = (self.output / "report.md").read_bytes()
        table = (self.output / "addresses.tsv").read_bytes()
        self.source.write_bytes(b"int recover_query() { return 2; }\n")
        with self.assertRaisesRegex(ValueError, "stale receipt; source changed"):
            self.run_progress(self.receipt)
        self.assertEqual((self.output / "report.md").read_bytes(), report)
        self.assertEqual((self.output / "addresses.tsv").read_bytes(), table)

    def test_complete_requires_dependency_function_record(self):
        self.functions = [self.functions[0]]
        declared = self.functions[0]
        declared["stages"] = {stage: True for stage in progress.STAGES}
        declared["dependencies"] = [{"address": SECOND, "boundary": "external",
                                      "status": "recovered"}]
        declared["open_obligations"] = []
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, "complete dependency record"):
            self.run_progress(self.receipt)
        self.assertFalse((self.output / "report.md").exists())


if __name__ == "__main__":
    unittest.main()
