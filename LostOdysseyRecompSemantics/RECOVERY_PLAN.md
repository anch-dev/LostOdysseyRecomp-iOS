# Semantic recovery plan

The user goal is complete semantic recovery of the game into human-readable, buildable C/C++ with behavior comparison against the original PPC before individual runtime replacements. The current work is the first staged slice of that whole-program goal and does not narrow it.

## Evidence gates

Each function advances independently through these gates:

1. **Identity** — match the XEX and generated-source evidence, and record provenance.
2. **Type and ABI** — establish guest addresses, endianness, calling convention, preserved registers and object boundaries.
3. **Control flow** — reproduce branches, loops, failure paths and return values.
4. **Side effects** — account for memory writes, ordering, cleanup and discarded results.
5. **Dependencies** — recover the contracts and effects of external callees.
6. **Behavior** — compare return values, guest-memory bytes, call traces and relevant snapshots through a bounded adapter.
7. **Runtime** — validate the adapter and behavior in representative scenes before considering a replacement.
8. **Complete** — mark the function complete only when the preceding evidence is sufficient for its scope.

The statuses in recovery.json are deliberately independent. A readable implementation does not imply differential, runtime, scene or complete status.

## Current order

The route is whole-program and staged: establish address/source identity at the boundary, deduplicate catalog candidates without erasing address identities, audit entry and body boundaries, then recover types and ABI, prioritize leaf functions and their dependencies, assemble validated subsystems, build adapters for behavior comparison, validate representative scenes, and replace runtime boundaries one at a time. This checkpoint retains eighty-two individually tracked readable entries alongside twenty-six family maps, with the combined records covering 4,963 unique mapped addresses and two overlaps. The runtime wrapper set remains 2,513 under seven independent gates, all disabled by default. One isolated all-gate D3D12 Map20/Uhra/menu smoke passed; the 58 getter entries were not individually traced. The incremental Windows native runtime/library build passed; details and receipts are in [registered_initializer_checkpoint.json](registered_initializer_checkpoint.json). The three bounded metadata entries passed five limited comparisons but retain a reproduced sNaN known mismatch and are not complete for all inputs. The accepted type-name index adds names for 847 existing constructor or lazy-singleton entries; see [REGISTERED_TYPES.md](REGISTERED_TYPES.md) and [registered_type_catalog.json](registered_type_catalog.json). It adds no recovery entries or validation counts. Constructor, callback, instance and metadata APIs remain library-only; the 58 getter entries have an optional runtime adapter without per-entry scene coverage. Earlier results were reused. Some cases still use bounded stubs or synthetic combinations. Exact-body families are evidence for implementation reuse only: keep every entry address mapped and validate its parameters against the original body, with representative behavior checks for the shared operation. Current work continues family and dependency recovery alongside runtime adapters; deeper vtable methods, CRT/native-kernel and unwind boundaries, ABI adapters and representative runtime scenes remain open. The free-return ABI, manager lifetime/concurrency and complete object contracts remain unresolved. The cache trace comparison does not establish hardware synchronization behavior. Generated PPC remains the differential baseline; the documented FP format model retains its signaling-NaN discrepancy separately in [FLOAT_LOAD_STORE_NOTES.md](FLOAT_LOAD_STORE_NOTES.md).

## Acceptance

A function is eligible for default runtime replacement when its identity, types, control flow, side effects and dependencies are documented; its bounded behavior comparison has a reproducible receipt; its runtime adapter is checked; and its scene evidence covers the intended scope. The source and bounded comparisons can be reviewed before runtime acceptance. A passing static-library build alone does not satisfy these gates. Runtime replacement, full-game recovery, frame-rate claims and cross-platform acceptance require separate evidence.

The current progress report covers eighty-two individually tracked entries and 4,963 unique mapped addresses across recovery.json and twenty-six family manifests. Details and bounded receipts are recorded in [registered_initializer_checkpoint.json](registered_initializer_checkpoint.json). The 847-entry type-name catalog is a descriptor-pointer naming index only. Runtime wrappers now total 2,513 under seven default-off gates; one isolated all-gate D3D12 Map20/Uhra/menu smoke passed, while the 58 getter entries were not individually traced; no registered-family runtime replacement is claimed for constructor, callback, instance or metadata APIs, and the new marker/metadata modules are outside that binary and scene scope. Complete semantics, MMIO, fault and concurrency remain unverified. Do not use WSL or Docker for this Windows-local workflow.
