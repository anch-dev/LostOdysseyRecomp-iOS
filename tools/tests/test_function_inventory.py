"""Boundary fixtures prevent evidence grouping from erasing callable entries."""

import importlib.util
from pathlib import Path
import sqlite3
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / 'ghidra/function_inventory.py'
SPEC = importlib.util.spec_from_file_location('function_inventory', SCRIPT)
inventory = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(inventory)


class FunctionInventoryTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.addCleanup(self.db.close)
        self.db.executescript('''
            CREATE TABLE metadata (key TEXT, value TEXT);
            CREATE TABLE functions (address TEXT PRIMARY KEY, name TEXT, is_thunk INTEGER);
            CREATE TABLE body_ranges (start INTEGER, end INTEGER, function_address TEXT);
            CREATE TABLE evidence (address TEXT, kind TEXT, path TEXT, line INTEGER);
        ''')
        self.db.execute('INSERT INTO metadata VALUES (?, ?)', ('xex_sha256', 'a' * 64))
        self.manifest = {'xex_sha256': 'a' * 64, 'functions': []}

    def function(self, addr, ranges, thunk=False):
        self.db.execute('INSERT INTO functions VALUES (?, ?, ?)', (f'{addr:08X}', 'function', thunk))
        self.db.executemany('INSERT INTO body_ranges VALUES (?, ?, ?)',
                            [(start, end, f'{addr:08X}') for start, end in ranges])

    def generated(self, addr, line=1):
        self.db.execute('INSERT INTO evidence VALUES (?, ?, ?, ?)',
                        (f'{addr:08X}', 'generated_ppc', 'fixture.cpp', line))

    def classify(self):
        summary, rows = inventory.classify(self.db, self.manifest)
        return summary, {row['address']: row for row in rows}

    def test_discontiguous_ranges_keep_gap_and_inclusive_end(self):
        self.function(0x82000000, [(0x82000000, 0x82000007), (0x82000020, 0x82000027)])
        for offset in (0, 7, 8, 0x20, 0x27, 0x28):
            self.generated(0x82000000 + offset)
        summary, rows = self.classify()
        self.assertEqual(summary['candidate_addresses'], 6)
        self.assertEqual(rows['82000007']['classification'], 'generated_interior_candidate')
        self.assertEqual(rows['82000008']['classification'], 'generated_uncovered_entry')
        self.assertEqual(rows['82000020']['body_owners'], ['82000000'])
        self.assertEqual(rows['82000027']['classification'], 'generated_interior_candidate')
        self.assertEqual(rows['82000028']['classification'], 'generated_uncovered_entry')

    def test_overlapping_ownership_is_ambiguous_not_merged(self):
        self.function(0x82000000, [(0x82000000, 0x8200001f)])
        self.function(0x82000010, [(0x82000010, 0x8200002f)])
        self.generated(0x82000014)
        self.generated(0x82000024)
        summary, rows = self.classify()
        self.assertEqual(rows['82000010']['classification'], 'conflicting_entry')
        self.assertEqual(rows['82000014']['classification'], 'ambiguous_interior_candidate')
        self.assertEqual(rows['82000014']['body_owners'], ['82000000', '82000010'])
        self.assertEqual(rows['82000024']['body_owners'], ['82000010'])
        self.assertEqual(summary['deleted_or_merged_addresses'], 0)
        self.assertIsNone(summary['unique_logical_function_total'])

    def test_repeated_records_collapse_only_same_address_and_keep_provenance(self):
        self.function(0x82000000, [(0x82000000, 0x82000003)], thunk=True)
        self.generated(0x82000000)
        self.generated(0x82000000, 10)
        self.generated(0x82000020, 20)
        self.manifest['functions'] = [{'address': '82000020'}]
        summary, rows = self.classify()
        self.assertEqual(summary['candidate_addresses'], 2)
        self.assertEqual(summary['generated_entries'], 2)
        self.assertEqual(summary['repeated_generated_address_records'], 1)
        self.assertEqual(summary['ghidra_thunks'], 1)
        self.assertEqual(len(rows['82000000']['generated_sources']), 2)
        self.assertTrue(rows['82000020']['has_recovery_record'])

    def test_ghidra_only_entry_does_not_become_generated(self):
        self.function(0x82000000, [])
        summary, rows = self.classify()
        self.assertEqual(summary['generated_entries'], 0)
        self.assertEqual(summary['shared_entry_addresses'], 0)
        self.assertEqual(rows['82000000']['classification'], 'ghidra_only_entry')
        self.assertFalse(rows['82000000']['generated_entry'])

    def test_identity_mismatch_and_unindexed_recovery_fail_closed(self):
        self.manifest['xex_sha256'] = 'b' * 64
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            self.classify()
        self.manifest['xex_sha256'] = 'a' * 64
        self.manifest['functions'] = [{'address': '82000000'}]
        with self.assertRaisesRegex(ValueError, 'unindexed'):
            self.classify()

    def test_invalid_ranges_fail_instead_of_repairing_inventory(self):
        self.function(0x82000000, [(0x82000004, 0x82000000)])
        with self.assertRaisesRegex(ValueError, 'body range'):
            self.classify()


if __name__ == '__main__':
    unittest.main()
