# Lost Odyssey semantic recovery library

[简体中文](README.zh-CN.md)

This directory is the first human-readable semantic-recovery library for the project. It is a standalone C++20 static library that can be configured with CMake without private game data, generated PPC code, a renderer or the game SDK. The Windows C++ toolchain still requires its normal compiler and SDK environment. The current source contains eighty-two individually tracked readable entries across query creation, query-pool allocation/release, allocator dispatch, memory services, guest-memory fill, cache-range handling, heap-core helpers, thread-state reporting, heap-allocation helpers, heap-growth/range/segment operations, guest memory move/copy, dynamic-array operations, special/raw allocation and manager construction, locking, storage initialization, manager allocation and resize paths, fallback resize, pointer-vector cleanup, manager buffer/array facades, object registration/startup, allocation-failure reporting and CRT services. Guest memory remains an explicit 32-bit big-endian window, and service boundaries remain opaque where their contracts are not yet recovered.

The library is a research and comparison surface. Runtime replacements remain disabled by default. The library does not claim complete recovery, compatibility, performance improvement or cross-platform validation. The current recovery record keeps readability, differential checks, runtime integration, scene validation and complete semantics as separate states; complete recovery is currently zero.

A separate leaf-family batch maps 942 original entrypoints to two shared readable C++ implementations (`PreserveR3` and `ReturnOne`) using three exact source templates. One native compilation and 4,710/4,710 complete PPCContext plus 4,096-byte ordinary-memory comparisons passed. The shared batch workflow removes per-function compile overhead, but no overall speedup is claimed; individually tracked implementations remain a separate count. The entry mapping is in [leaf_families.json](leaf_families.json). Reproduce it with `python -B tools/ghidra/test_semantic_leaf_family.py --write-map --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-leaf-family-tests"`. The runner requires the existing `out/function-inventory/exact-body-families.json` inventory and private inputs; use `--ppc-root <directory>` if the complete generated PPC lives in another checkout. This is bounded comparison evidence only: it does not enable runtime replacement or establish gameplay acceptance.

The twenty-six family manifests and 82 individual records in `recovery.json` cover 4,963 unique addresses after removing two overlaps: `829664E8` (individual/integer) and `82B84D88` (individual/global-assignment). This checkpoint adds 80 mapped entries: 13 component entries, 19 scalar entries, six UI entries and 42 default-field entries. These are mapped entry addresses, not independent logical functions or complete recovery.

The current initializer details and bounded receipts are recorded in [registered_initializer_checkpoint.json](registered_initializer_checkpoint.json). The FP helper/metadata scope retains a generated-C++ sNaN FPR gap; see [FLOAT_LOAD_STORE_NOTES.md](FLOAT_LOAD_STORE_NOTES.md). The component, scalar, UI and default-field modules have bounded library evidence with representative dynamic cases. The Windows native incremental library build and link passed; these results do not establish new runtime or game-scene acceptance. The accepted type-name index adds names for 847 existing constructor or lazy-singleton entries; see [REGISTERED_TYPES.md](REGISTERED_TYPES.md) and [registered_type_catalog.json](registered_type_catalog.json). It adds no recovery entries or validation counts.

Run only the focused comparison for changed behavior:

~~~powershell
python -B tools/ghidra/test_semantic_accessor_family.py --write-map --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-accessor-family-tests"
python -B tools/ghidra/test_semantic_integer_leaf.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-integer-leaf-tests"
python -B tools/ghidra/test_semantic_pointer_vector.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-pointer-vector-tests"
python -B tools/ghidra/test_semantic_field_bits.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-field-bits-tests"
python -B tools/ghidra/test_semantic_pointer_fields.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-pointer-fields-tests"
python -B tools/ghidra/test_semantic_allocation_failure.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-allocation-failure-tests"
python -B tools/ghidra/test_semantic_read_only_fields.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-read-only-field-tests"
python -B tools/ghidra/test_semantic_memory_writes.py --new-only --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-memory-write-tests"
python -B tools/ghidra/test_semantic_single_write_fields.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-single-write-tests"
python -B tools/ghidra/test_semantic_array_release_family.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-array-release-tests"
python -B tools/ghidra/test_semantic_registered_constructor_family.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-registered-constructor-tests"
python -B tools/ghidra/test_semantic_registered_callback_family.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-registered-callback-tests"
python -B tools/ghidra/test_semantic_registered_callback_family.py --scope dependency --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-registered-dependency-tests"
python -B tools/ghidra/test_semantic_registered_callback_family.py --scope composition --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-registered-composition-tests"
python -B tools/ghidra/test_semantic_registered_getter_family.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-registered-getter-tests"
python -B tools/ghidra/test_semantic_registered_runtime.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-registered-runtime-tests"
python -B tools/ghidra/test_semantic_crt_lifecycle.py --ppc-root <complete-ppc> --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-crt-lifecycle-tests"
~~~

The additional `827C4FA0` pointer-vector recovery releases cached pointers and zeros the count; its former GrowPointerVector boundary name was misleading. Ten boundary comparisons passed for return, memory and call order. The five allocation-failure helpers are `ReportRuntimeError` (`82B7FC98`), `ReportMissingHeapBanner` (`82B7FCE0`), `TerminateAllocationFailure` (`82B7BF20`), `InvokeNewHandler` (`82B7FE68`) and `GetAllocationErrorAddress` (`82B7FD78`). Their 18 standalone boundary cases and five raw-allocation compositions passed. ABI saves, volatile state and the real kernel remain excluded. The earlier 49-function hash-receipt command is historical and is not a current all-source freshness claim after shared-header changes.

Experimental runtime adapters now provide 2,513 wrappers behind seven default-off gates under `LO_ENABLE_SEMANTIC_RUNTIME`: `LO_SEMANTIC_LEAF_RUNTIME=1` selects 942 leaf wrappers, `LO_SEMANTIC_INTEGER_RUNTIME=1` selects 296 integer wrappers, `LO_SEMANTIC_ACCESSOR_RUNTIME=1` selects 304 accessor wrappers, `LO_SEMANTIC_FIELDS_RUNTIME=1` selects 242 field-bit/field-arithmetic wrappers, `LO_SEMANTIC_OBJECT_RUNTIME=1` selects 515 pointer-field/global-assignment/field-operation wrappers, and `LO_SEMANTIC_MEMORY_RUNTIME=1` selects 156 read-only/memory-write wrappers; `LO_SEMANTIC_REGISTERED_RUNTIME=1` selects the 58 registered-getter wrappers. All seven gates default to the original fallback. The object wrapper batch passed 2,060 original-PPC context/memory comparisons and the memory wrapper batch passed 624; earlier wrapper checks were reused. Array-release, manager-facade, object-registration/startup and CRT APIs have no runtime replacement yet.

The Windows RelWithDebInfo runtime increment compiled and linked with the new memory wrappers, reusing the supplied PPC archive. The earlier registered-constructor and registered-callback library-only checkpoint is retained as historical context; the 58 registered getters now have an optional runtime adapter and remain without per-entry scene coverage. The manager facades, object-registration/startup functions, array-release family and CRT APIs also remain library-only. One six-gate Windows D3D12 Uhra smoke at 1280x720/30 fps reached Map20 and opened the menu without an observed crash or visible structural regression; see [memory runtime checkpoint](runtime_memory_checkpoint.json). `LO_PREBUILT_RECOMP_LIBRARY` optionally reuses a supplied PPC archive, avoiding 246 PPC chunk compilations; `LO_PREBUILT_RECOMP_INCLUDE_DIR` defaults to the current PPC directory. An empty library variable retains source compilation.

The earlier original/recovered Windows D3D12 Uhra Residential Area (Map20) and main-menu smoke remains the comparison baseline in [runtime_checkpoint.json](runtime_checkpoint.json). The earlier four-gate run remains recorded in [runtime_field_checkpoint.json](runtime_field_checkpoint.json); the five-gate object run is recorded in [runtime_object_checkpoint.json](runtime_object_checkpoint.json), and the six-gate memory run in [runtime_memory_checkpoint.json](runtime_memory_checkpoint.json). These are bounded smoke evidence, not pixel identity, performance or per-entry acceptance. The earlier 2,455-wrapper baseline was not individually traced; the manager facades and eight object-registration/startup functions have no runtime replacement.

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
- 827C4FA0 FlushPointerVector
- 823F3340 ReleaseManagerBuffer
- 82486C88 AllocateManagerBuffer
- 823F3548 ClearBufferHeader
- 82298A98 ReleaseTwoByteArray
- 82298938 ResetTwoByteArray
- InitializeRegisteredObjectBase
- InitializeRegisteredObject
- ConstructRegisteredObject
- RegisterObjectGraph
- PrepareObject
- CompleteObjectStartup
- StartObject
- GetPrimaryRegisteredObject
The library also contains a PooledQueryServices composition adapter that connects recovered query creation, slot initialization and release inside the library; it is a composition surface, not runtime integration. The manager lifecycle composition passes 12 bounded cases across the original PPC bodies for InitializeManager, AllocateRawMemory, GetProcessHeap and the primary/fallback constructors. The manager-lock composition adds 12 cases and manager-startup adds 6 across their original/recovered bodies, with all three virtual calls resolved to real targets. The manager-allocation and fallback-resize compositions add 12 cases each; these are supplemental and do not add functions. The five manager buffer/array facades add 12 bounded original-PPC composition cases; their lower-level manager and array callees remain service stand-ins. The existing individually tracked additions have bounded receipts, including size-class boundary cases. The CRT batch adds 9 bounded cases, while ABI saved words and volatile context remain incomplete. Heap, CRT, native-critical-section and deeper vtable boundaries remain synthetic; generic ABI scratch and volatile context are excluded. Some tests use bounded stubs or synthetic combinations, so they do not establish complete memory recovery. Current work continues family and dependency recovery alongside runtime adapters; deeper vtable methods, CRT/native-kernel and unwind boundaries, ABI adapters and representative runtime scenes remain open. Expand by evidence and hotspots, rather than generating placeholder implementations for every address.
The earlier 49-function checkpoint recorded 48 ordinary PPC functions with 21,536 cases; query-lifecycle, heap-lifecycle, manager and fallback compositions brought that historical checkpoint to 21,665, while cache remained separate with 1,042 traces (1,040 complete and two limited-prefix). The current additions are tracked separately: the two CRT services add 9 bounded cases, global-assignment families add 416 complete PPCContext and ordinary-memory comparisons, and field-operation families add 536. Per-entry runtime and scene acceptance remain unset, complete recovery remains zero, and the cache result does not establish hardware synchronization.
The memory-fill comparison also exercises the unregistered PPCContext adapter, including store address, width, value and order. The adapter is evidence for the comparison harness only; it is not runtime integration. Function and field names are working names inferred from PPC evidence, not recovered original debug symbols.
The fill adapter does not prove the intermediate PPCContext state during a fault, and it has not been tested on an optimized or live path.

Reproduce the bounded oracle run from the repository root:

~~~powershell
python -B tools/ghidra/test_semantic_query.py --output "$env:USERPROFILE/worktrees/LostOdysseyRecomp/semantic-recovery-tests"
~~~

The earlier per-function receipt list and runner command are retained as historical evidence for the 49-function checkpoint; shared header changes mean they are not a current all-source freshness report.

The current progress report covers 82 individually tracked entries and 4,963 unique mapped addresses across `recovery.json` and twenty-six family manifests, after removing the two overlaps listed above. The incremental Windows native runtime/library build passed; details and receipts are recorded in [registered initializer checkpoint](registered_initializer_checkpoint.json). The `recovery.json` individual records retain unset per-entry runtime and scene acceptance; the 58 getter family entries have an optional runtime adapter, but no per-entry scene coverage. Runtime wrappers remain 2,513 behind seven default-off gates. A single isolated D3D12 Map20/Uhra/menu smoke passed with all seven gates enabled; the 58 getter entries were not individually traced, and no new broad scene acceptance is claimed. Complete status remains zero. Receipt outputs are kept outside the repository. The earlier 49-function report below remains a historical checkpoint:

The earlier read-only [function inventory audit](../tools/ghidra/README.md#deduplicate-before-expanding-recovery) remains a historical 62,627-address catalog/manifest snapshot: 50,412 shared entries, 7 Ghidra-only entries, 4 generated interior candidates and 12,204 generated uncovered entries. These categories do not establish the number of real logical functions or entry equivalence; exact-body families remain implementation-reuse candidates while every mapped entry is preserved and independently compared. Reuse this cached inventory while its underlying sources remain unchanged; extending recovery does not require another whole-source scan or hash pass.
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
The bounded comparison is evidence for its recorded scope only; it does not establish runtime integration, scene coverage, complete semantics or a performance change. The copy/move comparisons include final ordinary bytes, the r1-8 spill and the full 64-bit r3/count; their U64 implementation uses two U32 values and does not prove atomicity, fault behavior or access-width behavior. The array and free APIs are void semantic surfaces and do not expose the original residual r3; generic ABI scratch is excluded except for live locals and spills. The real copy/move composition inside array and special-allocation oracles is covered by their bounded receipts. The manager lifecycle receipt covers low addresses below 0xA0000 with nested-stack and seven high-address pages, full r3, callback arguments/order and before/after fingerprints, including primary/fallback/null/global mutation/frame+80 global alias and CRT retry paths. Its heap backend, CRT, kernel and virtual-method boundaries remain synthetic; real manager lifetime and concurrency remain pending. The Windows RelWithDebInfo runtime compile/link passed after an incremental build of the final modules and Semantics.lib/EXE link. The bounded Uhra/menu smoke passed for original and recovered paths in the same build; the later six-gate memory smoke also passed within its Map20/menu scope. Per-entry runtime behavior and scene acceptance remain unset, full semantics remain unverified, and runtime replacement remains disabled by default. See [runtime checkpoint](runtime_checkpoint.json), [field runtime checkpoint](runtime_field_checkpoint.json), [object runtime checkpoint](runtime_object_checkpoint.json) and [memory runtime checkpoint](runtime_memory_checkpoint.json).
The library composition fixture adds 74 query-lifecycle cases in `semantic-query-lifecycle-tests/receipt-827B7408-lifecycle.json`. The separate heap-lifecycle runner adds one multi-body Allocate→Free→Allocate sequence in `semantic-heap-lifecycle-tests/receipt.json`; the manager lifecycle runner adds 12 cases in `semantic-manager-lifecycle-tests/receipt-manager-lifecycle.json`; the manager-lock composition adds 12 cases in `semantic-manager-lock-tests/receipt-manager-lock-composed.json`; the manager-startup runner adds 6 cases in `semantic-manager-startup-tests/receipt-manager-startup.json`; manager-allocation composition adds 12 in `semantic-manager-allocate-tests/receipt-manager-allocate-composed.json`; and fallback-resize composition adds 12 in `semantic-fallback-resize-tests/receipt-fallback-resize-composed.json`. These are supplemental composition records and are intentionally excluded from the current per-function receipt list. The CRT services add 9 separate bounded cases. The startup runner reads `LostOdysseyRecompLib/private/image_disc1.bin` by default or accepts `--image <path>`, pins the XEX and image separately, and directly checks four slots. Its ten original/recovered bodies use real virtual targets; only heap/CRT/native-critical-section pieces are synthetic. The image decryption/unpacking source chain is not independently proved, and the result is not machine-code runtime validation. Generic ABI scratch and volatile context remain excluded; runtime, scene, complete semantics, MMIO, fault and concurrency validation remain open. The historical forty-eight ordinary PPC function comparisons totalled 21,536 cases; the current CRT services add 9 bounded cases, while global-assignment and field-operation families add 416 and 536 complete PPCContext and ordinary-memory comparisons respectively. Cache traces remain a separate 1,042-case result (1,040 complete and two limited-prefix).
See [RECOVERY_PLAN.md](RECOVERY_PLAN.md) for stage gates and [README.zh-CN.md](README.zh-CN.md) for the Chinese guide. recovery.json is the machine-readable status record. Preserve the original Ghidra project, generated PPC source and runtime implementation while this library is under review.
