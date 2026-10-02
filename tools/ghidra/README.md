# Ghidra scripts

The scripts in this directory work with the existing, privately maintained Ghidra project for the matching `default.xex`. The two analysis-library scripts below use `-noanalysis -readOnly` and do not persist changes to the project. The legacy address exporters may create functions or disassemble inside a temporary read-only transaction while producing their output. Ghidra, XEXLoaderWV, the JDK, the private project and the game executable are not distributed with this repository.

The older address-oriented exporters remain available:

| Script | Purpose |
|---|---|
| [ExportFunctions.java](ExportFunctions.java) | Export pseudo-C decompilation for one or more hexadecimal addresses |
| [ExportReferences.java](ExportReferences.java) | Export references for one or more hexadecimal addresses |
| [ExportAnalysisIndex.java](ExportAnalysisIndex.java) | Export existing function metadata, body ranges and resolved static calls as JSONL |
| [ExportDecompBatch.java](ExportDecompBatch.java) | Decompile only existing functions named by an address file |

## Match the source XEX first

The analysis project must be based on the same `default.xex` as the runtime/recompilation evidence. Record and compare the executable SHA-256 before using an index or decompilation output. A project that opens successfully is not evidence that it contains the right XEX. Keep historical source references whose identity has not been checked separate from a fresh export whose executable hash was checked.

The distinction between a requested address, a function entry point and a recompilation address matters. `ExportDecompBatch.java` accepts an address request and resolves it to an existing function containing that address; the emitted `address` is that function's entry point. A `recomp` symbol or hook address may therefore have a different boundary. Do not merge those identities without checking the source and call site.

## Export the existing analysis

Set the Windows paths for the local tools in PowerShell. These variables are examples; use the actual installation directories on the machine. WSL and Docker are not required.

```powershell
$env:GHIDRA_HOME = 'C:\Tools\ghidra_12.1.3_PUBLIC'
$env:JAVA_HOME = 'C:\Program Files\Java\jdk-21'
```

From the repository root, export the inventory without analysis:

```powershell
New-Item -ItemType Directory -Force out/decomp-index | Out-Null
.\tools\ghidra\headless.bat -process default.xex -noanalysis -readOnly `
  -postScript ExportAnalysisIndex.java out/decomp-index/inventory.jsonl
```

The JSONL contains a program record with the executable SHA-256, function records with body ranges, resolved static call records, and a final `complete` record. Check the program SHA-256 and the final completion record before consuming an inventory. A static call graph is necessarily incomplete: indirect and virtual calls may not resolve, and an absent edge is not proof that a call cannot occur.

## Export selected decompilation

Create a text file containing one hexadecimal address per line, then choose a new output directory. The target directory must not already exist; this prevents an earlier export from being silently mixed with a new one.

```powershell
(Get-Content tools/ghidra/annotations.json -Raw | ConvertFrom-Json).functions |
  ForEach-Object { $_.address } |
  Set-Content -Encoding ascii out/decomp-index/addresses.txt
.\tools\ghidra\headless.bat -process default.xex -noanalysis -readOnly `
  -postScript ExportDecompBatch.java `
  out/decomp-index/selected out/decomp-index/addresses.txt 45
```

The optional final argument is the per-function timeout in seconds and defaults to `45`. Only functions already present in the program are requested. The output contains one C file per successfully decompiled function and a `manifest.jsonl` recording `ok`, `missing` or `failed`. Verify the source XEX SHA-256 in the manifest and the final completion record, and retain the generated C files only as decompiler output: their presence and hash do not establish correct semantics. A missing generated file does not mean that the game lacks a function.

Ghidra headless may return exit code zero even when a post-script logs an error. Require the completed output and let the catalog validate its records and file hashes; do not use the shell exit code alone as evidence of success. An `ok` result means pseudocode was produced, including cases with warning comments or truncated control flow. The catalog retains these markers separately.

## Build and query the catalog

After the inventory and selected decompilation exist, [analysis_catalog.py](analysis_catalog.py) builds a local SQLite catalog. It also records the bounded annotations, PPC function locations, runtime source hook definitions and documentation address references that it can find. The scan is intentionally limited: it does not recursively capture arbitrary generated output. Keep generated `.jsonl`, `.sqlite`, `.c` and report files under ignored `out/`; they are local analysis artifacts and are not committed.

```powershell
python tools/ghidra/analysis_catalog.py build `
  --repo . `
  --inventory out/decomp-index/inventory.jsonl `
  --xex LostOdysseyRecompLib/private/disc1/default.xex `
  --annotations tools/ghidra/annotations.json `
  --output out/decomp-index/catalog.sqlite `
  --decomp-dir out/decomp-index/selected

python tools/ghidra/analysis_catalog.py query `
  --db out/decomp-index/catalog.sqlite `
  'gpu-query' --limit 20

python tools/ghidra/analysis_catalog.py show `
  --db out/decomp-index/catalog.sqlite 823B62A0

python tools/ghidra/analysis_catalog.py report `
  --db out/decomp-index/catalog.sqlite `
  --output out/decomp-index/report.md
```

The catalog is an index of evidence, not a proof of runtime behavior. A source hook proves that a definition exists in the checked source tree; it does not prove that the code compiled, was linked, or is enabled in the running build. Generated PPC code may be absent from the bounded scan even when the game has the corresponding behavior. Use the original source, the recompilation boundary and runtime evidence together when drawing a conclusion.

Repeat `--decomp-dir` to include additional completed batches. Rebuild the catalog after source or documentation edits so its file and line references reflect the current checkout. Legacy imports inspect only top-level `out/*.txt` and `out/*.c` files smaller than 4 MiB; their source version remains unverified. Generated PPC locations are also source correlations, not proof of an XEX provenance match.

The [semantic recovery library](../../LostOdysseyRecompSemantics/README.md) documents the staged human-readable recovery path. After its oracle receipt exists, [semantic_progress.py](semantic_progress.py) summarizes catalog candidates and independent recovery states:
The complete twenty-eight-receipt progress command is kept in the [semantic recovery guide](../../LostOdysseyRecompSemantics/README.md), using a portable PowerShell receipt list rather than the local untracked receipt-path JSON.

Progress is evidence bookkeeping: it does not turn generated candidates into recovered implementations or establish runtime replacement.
The semantic comparison runners for the current query-pool, cache, lifecycle, allocation-backend, memory-service, guest-fill, heap, thread-state, heap-allocation, growth, decommit, range, segment and heap-lifecycle slices are [test_semantic_query.py](test_semantic_query.py), [test_semantic_heap.py](test_semantic_heap.py), [test_semantic_heap_free.py](test_semantic_heap_free.py), [test_semantic_query_pool.py](test_semantic_query_pool.py), [test_semantic_cache.py](test_semantic_cache.py), [test_semantic_query_lifecycle.py](test_semantic_query_lifecycle.py), [test_semantic_allocation_backend.py](test_semantic_allocation_backend.py), [test_semantic_memory_services.py](test_semantic_memory_services.py), [test_semantic_memory_fill.py](test_semantic_memory_fill.py), [test_semantic_thread_state.py](test_semantic_thread_state.py), [test_semantic_heap_allocate.py](test_semantic_heap_allocate.py), [test_semantic_heap_growth.py](test_semantic_heap_growth.py), [test_semantic_heap_decommit.py](test_semantic_heap_decommit.py), [test_semantic_heap_ranges.py](test_semantic_heap_ranges.py), [test_semantic_heap_segment.py](test_semantic_heap_segment.py), and [test_semantic_heap_lifecycle.py](test_semantic_heap_lifecycle.py). They write receipts outside the repository; inspect completion records and source identity before using results. The query-lifecycle and heap-lifecycle receipts are additional composition evidence, not additional functions. The current semantic slice has twenty-eight readable and twenty-eight bounded-comparison functions; runtime, scene and complete states remain unverified.
From the repository root, the current slice can be rerun with these Windows-local commands:

~~~powershell
python -B tools/ghidra/test_semantic_query.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-recovery-tests"
python -B tools/ghidra/test_semantic_query_pool.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-recovery-pool-tests"
python -B tools/ghidra/test_semantic_cache.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-cache-tests"
python -B tools/ghidra/test_semantic_query_lifecycle.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-query-lifecycle-tests"
python -B tools/ghidra/test_semantic_allocation_backend.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-allocation-backend-tests"
python -B tools/ghidra/test_semantic_memory_services.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-memory-services-tests"
python -B tools/ghidra/test_semantic_memory_fill.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-memory-fill-tests"
python -B tools/ghidra/test_semantic_heap.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-heap-tests"
python -B tools/ghidra/test_semantic_heap_free.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-heap-free-tests"
python -B tools/ghidra/test_semantic_thread_state.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-thread-state-tests"
python -B tools/ghidra/test_semantic_heap_allocate.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-heap-allocate-tests"
python -B tools/ghidra/test_semantic_heap_growth.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-heap-growth-tests"
python -B tools/ghidra/test_semantic_heap_decommit.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-heap-decommit-tests"
python -B tools/ghidra/test_semantic_heap_ranges.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-heap-ranges-tests"
python -B tools/ghidra/test_semantic_heap_segment.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-heap-segment-tests"
python -B tools/ghidra/test_semantic_heap_lifecycle.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-heap-lifecycle-tests"
~~~

- The twenty-seven ordinary PPC function comparisons total 4,551 cases, the query-lifecycle composition receipt passes 74 cases, and the supplemental heap-lifecycle receipt passes one multi-body sequence, for 4,626 bounded comparison cases. The cache range result records 1,042 traces (1,040 complete and two limited-prefix). These results remain bounded evidence and do not establish complete memory recovery or runtime replacement.

The existing scripts can also be run directly:

```powershell
.\tools\ghidra\headless.bat -process default.xex -noanalysis -readOnly `
  -postScript ExportFunctions.java out/functions.txt 82A16DC0
```

Put all exports in ignored `out/`. Review decompiler output as a hypothesis and verify instruction semantics, data types, callers and side effects against the binary and runtime evidence. Ghidra's lack of a reference does not establish that no reference exists. Record reverse-engineering conclusions in the relevant [research notes](../../docs/notes/README.md).
