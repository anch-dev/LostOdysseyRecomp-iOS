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

### Deduplicate before expanding recovery

Use the read-only function inventory after the catalog and recovery manifest are available:

```powershell
python tools/ghidra/function_inventory.py `
  --catalog out/decomp-index/catalog.sqlite `
  --manifest LostOdysseyRecompSemantics/recovery.json `
  --output out/function-inventory
```

The current audit snapshot in `out/function-inventory/summary.json` has 62,627 union candidates: 50,412 `shared_entry`, 7 `ghidra_only_entry`, 4 `generated_interior_candidate` and 12,204 `generated_uncovered_entry`. It records 0 repeated generated-address records and 0 deleted-or-merged addresses. These are catalog/manifest classifications, not a count of real logical functions. The tool compares the XEX hash recorded by the catalog and manifest; it does not read or independently verify the actual XEX or the image provenance. Ghidra body ranges are analysis evidence and cannot prove entry equivalence, ABI equivalence, reachability, dead code or runtime behavior. The local `out/function-inventory/exact-body-families.json` audit retained instruction comments and all addresses while normalizing outer symbols and line endings. It found 61,597 normalized body texts across 62,620 generated addresses, with 48 repeated code groups covering 1,071 addresses and 1,023 reuse candidates; those families are only implementation-reuse evidence, not a final logical-function count or permission to merge entries. The four interior candidates each have external direct callers and an independently generated body and remain unsafe to merge, while the 12,204 uncovered entries remain unresolved candidates rather than duplicates; see `out/function-inventory/boundary-review.json`. Review these boundaries before expanding the recovery set; existing recovered functions are not rolled back.

The [semantic recovery library](../../LostOdysseyRecompSemantics/README.md) documents the staged human-readable recovery path. After its oracle receipt exists, [semantic_progress.py](semantic_progress.py) summarizes catalog candidates and independent recovery states:
The complete forty-nine-receipt progress command is kept in the [semantic recovery guide](../../LostOdysseyRecompSemantics/README.md), using a portable PowerShell receipt list rather than the local untracked receipt-path JSON.

Progress is evidence bookkeeping: it does not turn generated candidates into recovered implementations or establish runtime replacement.
The older forty-nine-function receipt set below is a historical checkpoint. The runner list's forty-nine-function wording is retained for that historical receipt set; current `HEAD` bookkeeping is 82 individually tracked readable entries across 90 family manifests and 5,180 unique mapped addresses. The CRT stream and formatting-support closure totals 26 entries, 1,086 static instructions, 88 focused original-PPC cases and nine unknown-entry checks; the separate array initializer adds one entry, 19 static instructions, four cases and one unknown check. See the [CRT stream checkpoint](../../LostOdysseyRecompSemantics/crt_stream_checkpoint.json), [CRT stream operations checkpoint](../../LostOdysseyRecompSemantics/crt_stream_operations_checkpoint.json) and [array code-unit conversion checkpoint](../../LostOdysseyRecompSemantics/array_code_unit_conversion_checkpoint.json). The `82B86BE0` conversion tail is absent from `catalog.functions`; the denominator remains the historical cached 62,627-address baseline. Runtime, scene and complete states remain unverified.
The semantic comparison runners for the current query-pool, cache, lifecycle, allocation-backend, memory-service, guest-fill, heap, thread-state, heap-allocation, growth, decommit, range, segment, memory-move, allocation-array, special-allocation, raw-allocation, manager-init, manager-construction, manager-lifecycle, manager-lock, manager-storage, manager-startup, manager-allocation, manager-resize, fallback-resize and heap-lifecycle slices are [test_semantic_query.py](test_semantic_query.py), [test_semantic_heap.py](test_semantic_heap.py), [test_semantic_heap_free.py](test_semantic_heap_free.py), [test_semantic_query_pool.py](test_semantic_query_pool.py), [test_semantic_cache.py](test_semantic_cache.py), [test_semantic_query_lifecycle.py](test_semantic_query_lifecycle.py), [test_semantic_allocation_backend.py](test_semantic_allocation_backend.py), [test_semantic_memory_services.py](test_semantic_memory_services.py), [test_semantic_memory_fill.py](test_semantic_memory_fill.py), [test_semantic_thread_state.py](test_semantic_thread_state.py), [test_semantic_heap_allocate.py](test_semantic_heap_allocate.py), [test_semantic_heap_growth.py](test_semantic_heap_growth.py), [test_semantic_heap_decommit.py](test_semantic_heap_decommit.py), [test_semantic_heap_ranges.py](test_semantic_heap_ranges.py), [test_semantic_heap_segment.py](test_semantic_heap_segment.py), [test_semantic_memory_move.py](test_semantic_memory_move.py), [test_semantic_allocation_array.py](test_semantic_allocation_array.py), [test_semantic_special_allocation.py](test_semantic_special_allocation.py), [test_semantic_raw_allocation.py](test_semantic_raw_allocation.py), [test_semantic_manager_init.py](test_semantic_manager_init.py), [test_semantic_manager_construction.py](test_semantic_manager_construction.py), [test_semantic_manager_lifecycle.py](test_semantic_manager_lifecycle.py), [test_semantic_manager_lock.py](test_semantic_manager_lock.py), [test_semantic_manager_storage.py](test_semantic_manager_storage.py), [test_semantic_manager_startup.py](test_semantic_manager_startup.py), [test_semantic_manager_allocate.py](test_semantic_manager_allocate.py), [test_semantic_manager_resize.py](test_semantic_manager_resize.py), [test_semantic_fallback_resize.py](test_semantic_fallback_resize.py), and [test_semantic_heap_lifecycle.py](test_semantic_heap_lifecycle.py). They write receipts outside the repository; inspect completion records and source identity before using results. The query-lifecycle, heap-lifecycle, manager-lifecycle, manager-lock, manager-startup, manager-allocation and fallback-resize receipts are additional composition evidence, not additional functions. The current semantic slice has forty-nine readable and forty-nine bounded-comparison functions; runtime, scene and complete states remain unverified.
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
python -B tools/ghidra/test_semantic_memory_move.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-memory-move-tests"
python -B tools/ghidra/test_semantic_allocation_array.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-allocation-array-tests"
python -B tools/ghidra/test_semantic_special_allocation.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-special-allocation-tests"
python -B tools/ghidra/test_semantic_raw_allocation.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-raw-allocation-tests"
python -B tools/ghidra/test_semantic_manager_init.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-manager-init-tests"
python -B tools/ghidra/test_semantic_manager_construction.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-manager-construction-tests"
python -B tools/ghidra/test_semantic_manager_lifecycle.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-manager-lifecycle-tests"
python -B tools/ghidra/test_semantic_manager_lock.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-manager-lock-tests"
python -B tools/ghidra/test_semantic_manager_storage.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-manager-storage-tests"
python -B tools/ghidra/test_semantic_manager_startup.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-manager-startup-tests"
python -B tools/ghidra/test_semantic_manager_allocate.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-manager-allocate-tests"
python -B tools/ghidra/test_semantic_manager_resize.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-manager-resize-tests"
python -B tools/ghidra/test_semantic_fallback_resize.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-fallback-resize-tests"
~~~

- The manager-startup runner does not require `out/decomp-index` or `manager-vtable-targets.json`; it reads the hash-pinned private unpacked image directly and accepts the default image path or an explicit `--image` path.
- The forty-eight ordinary PPC function comparisons total 21,536 cases; the query-lifecycle composition receipt passes 74 cases, the supplemental heap-lifecycle receipt passes one multi-body sequence, the manager-lifecycle receipt passes 12 cases, the manager-lock composition receipt passes 12 cases, the manager-startup receipt passes 6 cases, and the manager-allocation and fallback-resize compositions pass 12 cases each, for 21,665 bounded comparison cases. The six new functions have individual receipts; the two composition receipts are supplemental and excluded from the per-function list. The startup runner reads `LostOdysseyRecompLib/private/image_disc1.bin` by default or accepts `--image <path>`, pins the XEX and image separately, and checks four slots directly. Its ten original/recovered bodies resolve all three virtual calls to real targets; heap, CRT and native-critical-section pieces remain synthetic. The image decryption/unpacking provenance chain is not independently proved and the result is not machine-code runtime validation. Generic ABI scratch and volatile context remain excluded; real runtime, scene, complete semantics, MMIO, fault and concurrency validation remain pending. Copy/move comparisons cover final ordinary bytes, r1-8 spill and full 64-bit r3/count; the U64 model uses two U32 values and does not prove atomicity, fault behavior or access width. Void array/free APIs do not expose original residual r3. These results remain bounded evidence and do not establish complete memory recovery or runtime replacement.

The existing scripts can also be run directly:

```powershell
.\tools\ghidra\headless.bat -process default.xex -noanalysis -readOnly `
  -postScript ExportFunctions.java out/functions.txt 82A16DC0
```

Put all exports in ignored `out/`. Review decompiler output as a hypothesis and verify instruction semantics, data types, callers and side effects against the binary and runtime evidence. Ghidra's lack of a reference does not establish that no reference exists. Record reverse-engineering conclusions in the relevant [research notes](../../docs/notes/README.md).

## Shared semantic recovery workflow

For bounded family recovery, use the [semantic recovery workflow](RECOVERY_WORKFLOW.md) and the [worker packet template](templates/semantic_worker_prompt.md). The shared [semantic_recovery.py](semantic_recovery.py) helper provides body/metadata checks, one strict `--parallel 1` CMake library build, same-library oracle links, and HEAD-local progress reporting:

```powershell
python tools/ghidra/semantic_recovery.py check --manifest <paths> --ppc-root <path>
python tools/ghidra/semantic_recovery.py run --batch <root-relative.json> `
  --output <external-scratch> --library-build <configured-cmake-dir> `
  --msvc-runtime MT --library-file <exact-library.lib>
python tools/ghidra/semantic_recovery.py progress --runtime-wrappers 3168
```

`--msvc-runtime` must match the configured `MT`, `MD`, `MTd`, or `MDd` runtime. Use `--library-file` when standard library discovery is ambiguous. Outputs belong outside the checkout and ownCloud. Tool checks and shared-header fixtures validate the workflow; they do not add semantic recovery cases, prove complete PPC context, enable runtime replacement, or establish full-game behavior.
