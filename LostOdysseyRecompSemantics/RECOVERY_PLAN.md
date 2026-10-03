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

The route is whole-program and staged: establish address/source identity at the boundary, deduplicate catalog candidates without erasing address identities, audit entry and body boundaries, then recover types and ABI, prioritize leaf functions and their dependencies, assemble validated subsystems, build adapters for behavior comparison, validate representative scenes, and replace runtime boundaries one at a time. This checkpoint retains seventy-six individually tracked readable entries alongside twelve family maps, with the combined records covering 2,560 unique mapped addresses and two overlaps. The field-bit/field-arithmetic, pointer-field, global-assignment, field-operation and memory wrappers bring the runtime wrapper set to 2,455 under six independent gates, all disabled by default. The latest eight object-registration/startup functions passed 25 bounded comparisons, 31 array-release entries passed 31, and the 156 memory wrappers passed 624. Earlier results were reused. Some cases still use bounded stubs or synthetic combinations. Exact-body families are evidence for implementation reuse only: keep every entry address mapped and independently compared. Current work continues family and dependency recovery alongside runtime adapters; deeper vtable methods, CRT/native-kernel and unwind boundaries, ABI adapters and representative runtime scenes remain open. The free-return ABI, manager lifetime/concurrency and complete object contracts remain unresolved. The cache trace comparison does not establish hardware synchronization behavior. Generated PPC remains the baseline.

## Acceptance

A function is eligible for default runtime replacement when its identity, types, control flow, side effects and dependencies are documented; its bounded behavior comparison has a reproducible receipt; its runtime adapter is checked; and its scene evidence covers the intended scope. The source and bounded comparisons can be reviewed before runtime acceptance. A passing static-library build alone does not satisfy these gates. Runtime replacement, full-game recovery, frame-rate claims and cross-platform acceptance require separate evidence.

The current progress report covers seventy-six individually tracked entries and 2,560 unique mapped addresses across recovery.json and twelve family manifests (942 leaf, 304 accessor, 296 integer, 153 field-bit, 277 pointer-field, 104 global-assignment, 134 field-operation, 89 field-arithmetic, 39 read-only-field, 71 memory-write, 46 single-write-field and 31 array-release addresses, with overlaps at 829664E8 and 82B84D88). A Windows RelWithDebInfo complete runtime compile/link passed using the supplied local PPC archive; the prebuilt library variable remains optional and empty by default, while its include directory defaults to the current PPC directory. The earlier original/recovered Uhra Residential Area/menu smoke remains the comparison baseline. The object and memory runtime gates have bounded adapter comparisons and isolated five-/six-gate Map20/menu smokes, recorded in `runtime_object_checkpoint.json` and `runtime_memory_checkpoint.json`; the manager facades, eight object-registration/startup functions and array-release family are not covered by those scenes. This does not establish per-entry scene acceptance; runtime acceptance remains unset per entry, and the 2,455 wrappers were not each traced. See `runtime_checkpoint.json`, `runtime_field_checkpoint.json`, `runtime_object_checkpoint.json` and `runtime_memory_checkpoint.json` for the bounded smoke evidence. The standalone comparisons and family mappings remain bounded evidence; complete semantics, MMIO, fault and concurrency remain unverified. Do not use WSL or Docker for this Windows-local workflow.
