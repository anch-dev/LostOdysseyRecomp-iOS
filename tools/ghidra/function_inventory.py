#!/usr/bin/env python3
"""Audit duplicate address records and function-boundary disagreements read-only.

An entry inside a Ghidra body is a review candidate, not permission to erase a
generated function. Distinct entry points can have different calling contracts.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from contextlib import closing
import csv
import hashlib
import heapq
import json
from pathlib import Path
import re
import sqlite3


ROOT = Path(__file__).resolve().parents[2]


def address(value: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9A-F]{8}", value):
        raise ValueError(f"invalid catalog address: {value!r}")
    return value


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def classify(db: sqlite3.Connection, manifest: dict) -> tuple[dict, list[dict]]:
    metadata = dict(db.execute("SELECT key,value FROM metadata"))
    xex = metadata.get("xex_sha256", "")
    if not re.fullmatch(r"[0-9a-f]{64}", xex) or manifest.get("xex_sha256") != xex:
        raise ValueError("catalog and recovery manifest need the same valid XEX SHA-256")
    functions = {address(addr): (name, bool(thunk)) for addr, name, thunk in
                 db.execute("SELECT address,name,is_thunk FROM functions")}
    generated = defaultdict(list)
    for addr, path, line in db.execute(
            "SELECT address,path,line FROM evidence WHERE kind='generated_ppc' ORDER BY path,line"):
        generated[address(addr)].append(f"{path}:{line}")
    candidates = sorted(functions.keys() | generated.keys())
    recovered = [address(item['address']) for item in manifest['functions']]
    if len(set(recovered)) != len(recovered) or set(recovered) - set(candidates):
        raise ValueError("recovery manifest has duplicate or unindexed addresses")
    recovered = set(recovered)
    ranges = list(db.execute(
        "SELECT start,end,function_address FROM body_ranges ORDER BY start,end,function_address"))
    for start, end, owner in ranges:
        if (type(start) is not int or type(end) is not int or
                not 0 <= start <= end <= 0xffffffff or owner not in functions):
            raise ValueError(f"invalid function body range: {(start, end, owner)!r}")

    # The active interval sweep preserves gaps and overlapping ownership. A
    # min/max envelope would incorrectly absorb entries in discontiguous gaps.
    active = []
    owners = Counter()
    cursor = 0
    rows = []
    for addr in candidates:
        point = int(addr, 16)
        while cursor < len(ranges) and ranges[cursor][0] <= point:
            start, end, owner = ranges[cursor]
            heapq.heappush(active, (end, cursor, owner))
            owners[owner] += 1
            cursor += 1
        while active and active[0][0] < point:
            _, _, owner = heapq.heappop(active)
            owners[owner] -= 1
            if owners[owner] == 0:
                del owners[owner]
        containing = sorted(owners)
        if addr in functions:
            if any(owner != addr for owner in containing):
                classification = 'conflicting_entry'
            else:
                classification = 'shared_entry' if addr in generated else 'ghidra_only_entry'
        elif len(containing) == 1:
            classification = 'generated_interior_candidate'
        elif len(containing) > 1:
            classification = 'ambiguous_interior_candidate'
        else:
            classification = 'generated_uncovered_entry'
        rows.append(dict(address=addr, classification=classification,
                         ghidra_entry=addr in functions, generated_entry=addr in generated,
                         name=functions.get(addr, ('', False))[0],
                         is_thunk=functions.get(addr, ('', False))[1],
                         body_owners=containing, generated_sources=generated[addr],
                         has_recovery_record=addr in recovered))
    counts = Counter(row['classification'] for row in rows)
    generated_addresses = {row['address'] for row in rows if row['generated_entry']}
    summary = dict(
        schema_version=1, xex_sha256=xex,
        ghidra_entries=len(functions), generated_entries=len(generated_addresses),
        shared_entry_addresses=len(functions.keys() & generated_addresses),
        candidate_addresses=len(rows), classifications=dict(sorted(counts.items())),
        generated_occurrences=sum(len(row['generated_sources']) for row in rows),
        repeated_generated_address_records=sum(max(0, len(row['generated_sources']) - 1) for row in rows),
        ghidra_thunks=sum(row['is_thunk'] for row in rows),
        recovered_addresses=len(recovered),
        interior_candidates=[row for row in rows if 'interior' in row['classification']],
        conflicting_entries=[row for row in rows if row['classification'] == 'conflicting_entry'],
        unique_logical_function_total=None,
        deleted_or_merged_addresses=0,
        limits=[
            'XEX identity is compared between catalog and manifest records; original image bytes and generated-source provenance are not revalidated by this audit.',
            'Same-address source records are already combined in the candidate union.',
            'Ghidra body ownership is analysis evidence, not proof that an alternate entry has the same ABI or semantics.',
            'Uncovered generated entries remain unresolved, not duplicates or dead code.',
            'Thunks and identical code at different addresses retain separate address identities.',
            'Resolved static calls omit unresolved indirect targets and do not prove unreachability.',
            'No source, function boundary, hook or recovery status is changed by this audit.',
        ])
    assert sum(counts.values()) == len(rows)
    return summary, rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'LostOdysseyRecompSemantics/recovery.json')
    parser.add_argument('--output', type=Path, default=ROOT / 'out/function-inventory')
    args = parser.parse_args()
    catalog, manifest_path = args.catalog.resolve(), args.manifest.resolve()
    output = args.output.resolve()
    files = {name: output / name for name in ('summary.json', 'addresses.tsv', 'report.md')}
    if {catalog, manifest_path} & set(files.values()):
        parser.error('output would overwrite an input')
    before = digest(catalog)
    manifest_bytes = manifest_path.read_bytes()
    with closing(sqlite3.connect(catalog.as_uri() + '?mode=ro', uri=True)) as db:
        summary, rows = classify(db, json.loads(manifest_bytes))
    if digest(catalog) != before:
        raise ValueError('catalog changed while the audit was reading it')
    summary['catalog'] = dict(path=str(catalog), sha256=before)
    summary['manifest'] = dict(path=str(manifest_path), sha256=hashlib.sha256(manifest_bytes).hexdigest())
    output.mkdir(parents=True, exist_ok=True)
    with files['addresses.tsv'].open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]) if rows else [
            'address', 'classification', 'ghidra_entry', 'generated_entry', 'name',
            'is_thunk', 'body_owners', 'generated_sources', 'has_recovery_record'], delimiter='\t')
        writer.writeheader()
        for row in rows:
            writer.writerow({key: ';'.join(value) if isinstance(value, list) else value
                             for key, value in row.items()})
    lines = ['# Function boundary inventory', '',
             'This is a read-only classification, not a confirmed count of logical functions.', '',
             '| Inventory | Count |', '| --- | ---: |',
             f"| Ghidra entry addresses | {summary['ghidra_entries']:,} |",
             f"| Generated PPC entry addresses | {summary['generated_entries']:,} |",
             f"| Shared addresses already deduplicated | {summary['shared_entry_addresses']:,} |",
             f"| Candidate address union | {summary['candidate_addresses']:,} |", '',
             '| Classification | Count |', '| --- | ---: |']
    lines += [f'| {kind} | {count:,} |' for kind, count in summary['classifications'].items()]
    lines += ['', '## Interior entries requiring review', '']
    for row in summary['interior_candidates']:
        lines.append(f"- `{row['address']}` lies in {', '.join(row['body_owners'])}; retain its entry contract until reviewed.")
    if not summary['interior_candidates']:
        lines.append('- None in the current body ranges.')
    lines += ['', '## Interpretation', '',
              'Review boundary conflicts and interior entry contracts before grouping recovery tasks. '
              'Check uncovered entries against original bytes and control flow. '
              'Preserve distinct thunks and address-sensitive contracts. '
              'No authoritative logical-function total or recovery percentage is established.', '']
    lines += [f'- {limit}' for limit in summary['limits']]
    files['report.md'].write_text('\n'.join(lines) + '\n', encoding='utf-8')
    files['summary.json'].write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: summary[key] for key in (
        'candidate_addresses', 'shared_entry_addresses', 'classifications',
        'repeated_generated_address_records', 'unique_logical_function_total')}, indent=2))


if __name__ == '__main__':
    main()
