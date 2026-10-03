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

The route is whole-program and staged: establish address/source identity at the boundary, deduplicate catalog candidates without erasing address identities, audit entry and body boundaries, then recover types and ABI, prioritize leaf functions and their dependencies, assemble validated subsystems, build adapters for behavior comparison, validate representative scenes, and replace runtime boundaries one at a time. This checkpoint retains sixty-three individually tracked readable entries alongside ten family maps, with the combined records covering 2,427 unique mapped addresses and two overlaps. The field-bit/field-arithmetic wrappers bring the existing runtime wrapper set to 1,784 under four independent gates, all disabled by default. The read-only-field and memory-write batches add 234 and 168 bounded comparisons; the composed CRT lifecycle paths add three. Some cases still use bounded stubs or synthetic combinations. Exact-body families are evidence for implementation reuse only: keep every entry address mapped and independently compared. Current work continues family and dependency recovery alongside runtime adapters; deeper vtable methods, CRT/native-kernel and unwind boundaries, ABI adapters and representative runtime scenes remain open. The free-return ABI, manager lifetime/concurrency and complete object contracts remain unresolved. The cache trace comparison does not establish hardware synchronization behavior. Generated PPC remains the baseline.

## Acceptance

A function is eligible for default runtime replacement when its identity, types, control flow, side effects and dependencies are documented; its bounded behavior comparison has a reproducible receipt; its runtime adapter is checked; and its scene evidence covers the intended scope. The source and bounded comparisons can be reviewed before runtime acceptance. A passing static-library build alone does not satisfy these gates. Runtime replacement, full-game recovery, frame-rate claims and cross-platform acceptance require separate evidence.

The current progress report covers sixty-three individually tracked entries and 2,427 unique mapped addresses across recovery.json and ten family manifests (942 leaf, 304 accessor, 296 integer, 153 field-bit, 277 pointer-field, 104 global-assignment, 134 field-operation, 89 field-arithmetic, 39 read-only-field and 28 memory-write addresses, with overlaps at 829664E8 and 82B84D88). A Windows RelWithDebInfo complete runtime compile/link passed using the supplied local PPC archive; the prebuilt library variable remains optional and empty by default, while its include directory defaults to the current PPC directory. The earlier original/recovered Uhra Residential Area/menu smoke remains the comparison baseline. One additional isolated run enabled all four runtime gates, reached Map20 and opened the main menu without an observed crash or visible structural regression. This was not pixel identity, performance testing or per-entry scene acceptance; runtime acceptance remains unset per entry, and the 1,784 wrappers were not each traced. See `runtime_checkpoint.json` and `runtime_field_checkpoint.json` for the smoke evidence. The standalone comparisons and family mappings remain bounded evidence; complete semantics, MMIO, fault and concurrency remain unverified. Do not use WSL or Docker for this Windows-local workflow.
