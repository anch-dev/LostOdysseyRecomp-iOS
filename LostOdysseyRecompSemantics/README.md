# Lost Odyssey semantic recovery library

This directory is the first human-readable semantic-recovery library for the project. It is a standalone C++20 static library that can be configured with CMake without private game data, generated PPC code, a renderer or the game SDK. The Windows C++ toolchain still requires its normal compiler and SDK environment. The current source contains forty-nine readable implementations across query creation, query-pool allocation/release, allocator dispatch, memory services, guest-memory fill, cache-range handling, heap-core helpers, thread-state reporting, heap-allocation helpers, heap-growth/range/segment operations, guest memory move/copy, dynamic-array operations, special/raw allocation and manager construction, locking, storage initialization, manager allocation and resize paths, and fallback resize. Guest memory remains an explicit 32-bit big-endian window, and service boundaries remain opaque where their contracts are not yet recovered.

The library is a research and comparison surface. It does not replace the generated runtime, enable a runtime hook, or claim complete recovery, compatibility, performance improvement or cross-platform validation. The current recovery record keeps readability, differential checks, runtime integration, scene validation and complete semantics as separate states; complete recovery is currently zero.

## Build

From a Windows cmd shell, use the repository setup helper once, then configure and build the standalone directory outside the ownCloud checkout:

~~~cmd
call tools\setup_windows.bat
cmake -S LostOdysseyRecompSemantics -B "%USERPROFILE%\worktrees\LostOdysseyRecomp\semantic-recovery-isolated-library" -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build "%USERPROFILE%\worktrees\LostOdysseyRecomp\semantic-recovery-isolated-library"
~~~

The same CMake commands can run in a Visual Studio developer terminal. The setup helper selects existing local tools; it does not install dependencies. A root build can expose the optional target through LO_BUILD_SEMANTIC_LIBRARY, whose default is OFF.

## Recovery scope

GuestMemory models a bounded Xbox guest-address window and reads/writes big-endian words explicitly. QueryServices keeps the external callees opaque until their contracts are recovered. CreateType9Query records the observed allocation size and flags, store order, slot loop, failure cleanup and return behavior. The original generated PPC and the decompiler output remain the evidence sources; readable C++ is not accepted as proof by itself.

The recovery sequence is:

1. establish XEX and generated-source identity;
2. recover types, ABI and control flow;
3. recover side effects and external dependencies;
4. compare behavior through an adapter and bounded guest-memory model;
5. validate representative runtime scenes;
6. replace one runtime boundary only after the preceding evidence is complete.

The current readable slice is:

- 827B7408 CreateType9Query
- 827B72E8 InitializeQuerySlot
- 823CDCA8 ReleaseQuery
- 827C9D88 AllocateDispatch
- 827C9DB0 FreeDispatch
- 823EA178 FlushDataCacheRange
- 827CA050 AllocateGeneral
- 827CA0E8 FreeGeneral
- 827C9E20 AllocatePhysicalMemory
- 827C9EB8 FreePhysicalMemory
- 827CAD38 AllocateHeapMemory
- 827CAD80 FreeHeapMemory
- 823ACC98 GetProcessHeap
- 82B7BC40 FillGuestMemory
- 827CBA60 InsertFreeBlocks
- 823AE108 CoalesceFreeBlocks
- 823ADE28 FreeHeapBlock
- 823AE0BC LeaveHeapCriticalSection
- 822CA180 ReportAllocationFailure
- 822CA188 StoreThreadFailureCode
- 823ACCB0 AllocateHeapBlock
- 823AD544 LeaveHeapAllocateCriticalSection
- 827CC428 GrowHeap
- 827CC668 DecommitFreeBlock
- 827CB498 AllocateRangeNode
- 827CB658 InsertRangeRecord
- 827CB778 ExtendHeapSegment
- 827CC2C0 InitializeHeapSegment
- 82B7A0B0 CopyGuestMemory
- 82B7C470 MoveGuestMemory
- 8229F678 ResizeArray
- 82298AF8 RemoveArrayRange
- 827C9A40 AllocateSpecialBlock
- 827C9C60 FreeSpecialBlock
- 823ACBD0 AllocateRawMemory
- 827C5F38 InitializeManager
- 827C5970 ConstructPrimaryManager
- 827C4ED0 ConstructFallbackManager
- 829664E8 ReturnZeroStatus
- 822958F8 StoreLockAndWaitForEnter
- 827C5688 InvokeManagerUnderLock
- 827C5B30 InitializeStorageBuckets
- 827C5D88 InitializePrimaryManagerStorage
- 823F2670 AllocateThroughPrimaryManager
- 823F25E0 AllocateThroughFallbackManager
- 82295950 ResizePrimaryManagerStorage
- 822A0738 FindPrimaryResizeNode
- 82295530 ResizeFallbackAllocation
- 827C5050 AppendPointerToVector
The library also contains a PooledQueryServices composition adapter that connects recovered query creation, slot initialization and release inside the library; it is a composition surface, not runtime integration. The manager lifecycle composition passes 12 bounded cases across the original PPC bodies for InitializeManager, AllocateRawMemory, GetProcessHeap and the primary/fallback constructors. The manager-lock composition adds 12 cases and manager-startup adds 6 across their original/recovered bodies, with all three virtual calls resolved to real targets. The manager-allocation and fallback-resize compositions add 12 cases each; these are supplemental and do not add functions. The six new functions have individual bounded receipts, including size-class boundary cases. Heap, CRT, native-critical-section and deeper vtable boundaries remain synthetic; generic ABI scratch and volatile context are excluded. Some tests use bounded stubs or synthetic combinations, so they do not establish complete memory recovery. The next priority remains entry and boundary audit after inventory, then deeper vtable methods, CRT/native-kernel and unwind boundaries, ABI adapters and representative runtime scenes. Expand by evidence and hotspots, rather than generating placeholder implementations for every address.
This checkpoint adds twenty-one readable implementations and twenty-one bounded behavior records overall, bringing the current source and oracle set to forty-nine each. The forty-eight ordinary PPC functions account for 21,536 cases; the cache result remains separate with 1,042 traces (1,040 complete and two limited-prefix). Query-lifecycle adds 74 cases, the supplemental heap-lifecycle runner adds one sequence, manager lifecycle adds 12, manager-lock composition adds 12, manager-startup adds 6, manager-allocation composition adds 12 and fallback-resize composition adds 12, for 21,665 bounded comparison cases. Runtime, scene and complete remain zero, and the cache result does not establish hardware synchronization.
The memory-fill comparison also exercises the unregistered PPCContext adapter, including store address, width, value and order. The adapter is evidence for the comparison harness only; it is not runtime integration. Function and field names are working names inferred from PPC evidence, not recovered original debug symbols.
The fill adapter does not prove the intermediate PPCContext state during a fault, and it has not been tested on an optimized or live path.

Reproduce the bounded oracle run from the repository root:

~~~powershell
python -B tools/ghidra/test_semantic_query.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-recovery-tests"
~~~

Run the remaining [semantic comparison runners](../tools/ghidra/README.md) before collecting the report below; its receipt list requires all forty-nine per-function results.

The current progress report covers 62,627 candidate addresses, with forty-nine readable and forty-nine bounded-comparison functions and zero runtime, scene or complete functions. The receipt is kept outside the repository. To summarize it with the catalog:

Before expanding this set, run the read-only [function inventory audit](../tools/ghidra/README.md#deduplicate-before-expanding-recovery). Its current snapshot is a 62,627-address catalog/manifest union: 50,412 shared entries, 7 Ghidra-only entries, 4 generated interior candidates and 12,204 generated uncovered entries. These categories do not establish the number of real logical functions or entry equivalence; the audit does not independently verify the actual XEX or image provenance and does not change recovery records. The four interior candidates each have external direct callers and an independently generated body and are unsafe to merge; the 12,204 uncovered entries are unresolved candidates, not duplicate records. The boundary review remains in `out/function-inventory/boundary-review.json`. Exact-body analysis found 61,597 distinct generated body texts, with 48 repeated groups covering 1,071 entries and 1,023 potential repeated implementations. Treat these as implementation-reuse candidates while preserving and independently comparing every entry; they do not establish a logical-function total.
~~~powershell
$evidenceRoot = Join-Path $env:USERPROFILE 'worktrees/LostOdysseyRecomp'
$receiptRel = @(
  'semantic-recovery-tests\receipt.json',
  'semantic-recovery-pool-tests\receipt-823CDCA8.json',
  'semantic-recovery-pool-tests\receipt-827B72E8.json',
  'semantic-recovery-pool-tests\receipt-827C9D88.json',
  'semantic-recovery-pool-tests\receipt-827C9DB0.json',
  'semantic-cache-tests\receipt.json',
  'semantic-allocation-backend-tests\receipt-827CA050.json',
  'semantic-allocation-backend-tests\receipt-827CA0E8.json',
  'semantic-memory-services-tests\receipt-823ACC98.json',
  'semantic-memory-services-tests\receipt-827C9E20.json',
  'semantic-memory-services-tests\receipt-827C9EB8.json',
  'semantic-memory-services-tests\receipt-827CAD38.json',
  'semantic-memory-services-tests\receipt-827CAD80.json',
  'semantic-memory-fill-tests\receipt.json',
  'semantic-heap-tests\receipt-823AE108.json',
  'semantic-heap-tests\receipt-827CBA60.json',
  'semantic-heap-free-tests\receipt-823ADE28.json',
  'semantic-heap-free-tests\receipt-823AE0BC.json',
  'semantic-thread-state-tests\receipt-822CA180.json',
  'semantic-thread-state-tests\receipt-822CA188.json',
  'semantic-heap-allocate-tests\receipt-823ACCB0.json',
  'semantic-heap-allocate-tests\receipt-823AD544.json',
  'semantic-heap-growth-tests\receipt-827CC428.json',
  'semantic-heap-decommit-tests\receipt-827CC668.json',
  'semantic-heap-ranges-tests\receipt-827CB498.json',
  'semantic-heap-ranges-tests\receipt-827CB658.json',
  'semantic-heap-segment-tests\receipt-827CB778.json',
  'semantic-heap-segment-tests\receipt-827CC2C0.json',
  'semantic-memory-move-tests\receipt-82B7A0B0.json',
  'semantic-memory-move-tests\receipt-82B7C470.json',
  'semantic-allocation-array-tests\receipt-8229F678.json',
  'semantic-allocation-array-tests\receipt-82298AF8.json',
  'semantic-special-allocation-tests\receipt-827C9A40.json',
  'semantic-special-allocation-tests\receipt-827C9C60.json',
  'semantic-raw-allocation-tests\receipt-823ACBD0.json',
  'semantic-manager-init-tests\receipt-827C5F38.json',
  'semantic-manager-construction-tests\receipt-827C5970.json',
  'semantic-manager-construction-tests\receipt-827C4ED0.json',
  'semantic-manager-lock-tests\receipt-829664E8.json',
  'semantic-manager-lock-tests\receipt-822958F8.json',
  'semantic-manager-lock-tests\receipt-827C5688.json',
  'semantic-manager-storage-tests\receipt-827C5B30.json',
  'semantic-manager-storage-tests\receipt-827C5D88.json',
  'semantic-manager-allocate-tests\receipt-823F2670.json',
  'semantic-manager-allocate-tests\receipt-823F25E0.json',
  'semantic-manager-resize-tests\receipt-82295950.json',
  'semantic-manager-resize-tests\receipt-822A0738.json',
  'semantic-fallback-resize-tests\receipt-82295530.json',
  'semantic-fallback-resize-tests\receipt-827C5050.json'
)
$receiptArgs = foreach ($relative in $receiptRel) { '--receipt'; Join-Path $evidenceRoot $relative }
python tools/ghidra/semantic_progress.py `
  --catalog out/decomp-index/catalog.sqlite `
  --manifest LostOdysseyRecompSemantics/recovery.json `
  @receiptArgs `
  --output out/semantic-recovery
~~~
The bounded comparison is evidence for its recorded scope only; it does not establish runtime integration, scene coverage, complete semantics or a performance change. The copy/move comparisons include final ordinary bytes, the r1-8 spill and the full 64-bit r3/count; their U64 implementation uses two U32 values and does not prove atomicity, fault behavior or access-width behavior. The array and free APIs are void semantic surfaces and do not expose the original residual r3; generic ABI scratch is excluded except for live locals and spills. The real copy/move composition inside array and special-allocation oracles is covered by their bounded receipts. The manager lifecycle receipt covers low addresses below 0xA0000 with nested-stack and seven high-address pages, full r3, callback arguments/order and before/after fingerprints, including primary/fallback/null/global mutation/frame+80 global alias and CRT retry paths. Its heap backend, CRT, kernel and virtual-method boundaries remain synthetic; real manager lifetime and concurrency remain pending. The native Windows Release static-library build passed with all 49 recovered functions, but there is no runtime integration, scene validation or full-semantics result, and no hook is enabled.
The library composition fixture adds 74 query-lifecycle cases in `semantic-query-lifecycle-tests/receipt-827B7408-lifecycle.json`. The separate heap-lifecycle runner adds one multi-body Allocate→Free→Allocate sequence in `semantic-heap-lifecycle-tests/receipt.json`; the manager lifecycle runner adds 12 cases in `semantic-manager-lifecycle-tests/receipt-manager-lifecycle.json`; the manager-lock composition adds 12 cases in `semantic-manager-lock-tests/receipt-manager-lock-composed.json`; the manager-startup runner adds 6 cases in `semantic-manager-startup-tests/receipt-manager-startup.json`; manager-allocation composition adds 12 in `semantic-manager-allocate-tests/receipt-manager-allocate-composed.json`; and fallback-resize composition adds 12 in `semantic-fallback-resize-tests/receipt-fallback-resize-composed.json`. These are supplemental composition records and are intentionally excluded from the forty-nine per-function receipt list. The six new functions have individual receipts, including size-class boundary cases. The startup runner reads `LostOdysseyRecompLib/private/image_disc1.bin` by default or accepts `--image <path>`, pins the XEX and image separately, and directly checks four slots. Its ten original/recovered bodies use real virtual targets; only heap/CRT/native-critical-section pieces are synthetic. The image decryption/unpacking source chain is not independently proved, and the result is not machine-code runtime validation. Generic ABI scratch and volatile context remain excluded; runtime, scene, complete semantics, MMIO, fault and concurrency validation remain open. The forty-eight ordinary PPC function comparisons total 21,536 cases; with the 74 query-lifecycle cases, one heap-lifecycle sequence, 12 manager-lifecycle cases, 12 manager-lock cases, 6 manager-startup cases, 12 manager-allocation cases and 12 fallback-resize cases they total 21,665, while cache traces remain a separate 1,042-case result (1,040 complete and two limited-prefix).
See [RECOVERY_PLAN.md](RECOVERY_PLAN.md) for stage gates and [README.zh-CN.md](README.zh-CN.md) for the Chinese guide. recovery.json is the machine-readable status record. Preserve the original Ghidra project, generated PPC source and runtime implementation while this library is under review.
