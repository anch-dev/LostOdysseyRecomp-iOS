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

The route is whole-program and staged: establish address/source identity at the boundary, deduplicate catalog candidates without erasing address identities, audit entry and body boundaries, then recover types and ABI, prioritize leaf functions and their dependencies, assemble validated subsystems, build adapters for behavior comparison, validate representative scenes, and replace runtime boundaries one at a time. This checkpoint retains seventy-seven individually tracked readable entries alongside sixteen family maps, with the combined records covering 4,217 unique mapped addresses and two overlaps. The field-bit/field-arithmetic, pointer-field, global-assignment, field-operation and memory wrappers bring the runtime wrapper set to 2,455 under six independent gates, all disabled by default. The current registered-object batch passed 1,692 registered-constructor, 1,432 registered-callback, 105 registered-getter and 109 dependency-callback comparisons; four affected callback compositions passed 13 representative comparisons. These families have no runtime adapters; details and receipts are in [registered_object_checkpoint.json](registered_object_checkpoint.json). Earlier results were reused. Some cases still use bounded stubs or synthetic combinations. Exact-body families are evidence for implementation reuse only: keep every entry address mapped and independently compared. Current work continues family and dependency recovery alongside runtime adapters; deeper vtable methods, CRT/native-kernel and unwind boundaries, ABI adapters and representative runtime scenes remain open. The free-return ABI, manager lifetime/concurrency and complete object contracts remain unresolved. The cache trace comparison does not establish hardware synchronization behavior. Generated PPC remains the baseline.

## Acceptance

A function is eligible for default runtime replacement when its identity, types, control flow, side effects and dependencies are documented; its bounded behavior comparison has a reproducible receipt; its runtime adapter is checked; and its scene evidence covers the intended scope. The source and bounded comparisons can be reviewed before runtime acceptance. A passing static-library build alone does not satisfy these gates. Runtime replacement, full-game recovery, frame-rate claims and cross-platform acceptance require separate evidence.

The current progress report covers seventy-seven individually tracked entries and 4,217 unique mapped addresses across recovery.json and sixteen family manifests. The registered-object details and bounded receipts are recorded in [registered_object_checkpoint.json](registered_object_checkpoint.json). A Windows RelWithDebInfo semantics-library build passed, while runtime wrappers remain 2,455 under six default-off gates and no registered-family runtime replacement is claimed. The earlier six-gate Map20/menu smoke covers existing runtime wrappers only; this increment added no new scene validation and does not cover registered families. Complete semantics, MMIO, fault and concurrency remain unverified. Do not use WSL or Docker for this Windows-local workflow.
