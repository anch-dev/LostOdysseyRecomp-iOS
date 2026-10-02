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

The route is whole-program and staged: establish address/source identity at the boundary, recover types and ABI, prioritize leaf functions and their dependencies, assemble validated subsystems, build adapters for behavior comparison, validate representative scenes, then replace runtime boundaries one at a time. The current checkpoint covers twenty-eight readable functions: query creation, query-pool initialization/release, allocator dispatch, general/physical/heap memory services, guest-memory fill, cache-range flushing, heap-core helpers, thread-state reporting, heap allocation critical-section helpers and heap growth/range/segment operations. Some heap cases still use bounded stubs or synthetic range combinations. The next depth is the special allocator at 827C9A40/827C9C60: investigate the guest dynamic-array path, global manager 0x8330B608, lazy initialization 827C5F38, array removal 82298AF8, memmove 82B7C470, optional resize 8229F678 and the vtable resize/allocate slots; the free-return ABI remains unresolved. Indirect commit/manager paths, native kernel lifetimes, unwind and runtime boundaries remain open as well. The cache trace comparison does not establish hardware synchronization behavior. Generated PPC remains the baseline.

## Acceptance

A function is eligible for default runtime replacement when its identity, types, control flow, side effects and dependencies are documented; its bounded behavior comparison has a reproducible receipt; its runtime adapter is checked; and its scene evidence covers the intended scope. The source and bounded comparisons can be reviewed before runtime acceptance. A passing static-library build alone does not satisfy these gates. Runtime replacement, full-game recovery, frame-rate claims and cross-platform acceptance require separate evidence.

The current progress report covers 62,627 candidate addresses, with twenty-eight readable and twenty-eight bounded-comparison functions and zero runtime, scene or complete functions. The twenty-seven ordinary PPC functions account for 4,551 cases; query-lifecycle composition adds 74 cases, and the supplemental heap-lifecycle runner adds one multi-body Allocate-to-Free-to-Allocate sequence, for 4,626 bounded comparison cases. The heap sequence checks the observed Grow, Initialize, Extend, Decommit and first Fill steps, while its kernel/process/lock portions remain synthetic; runtime concurrency and physical-page state are unverified. The comparison helper uses ignored PPC source as an oracle and records source hashes, preserved registers, return values, guest-memory effects, call traces and snapshots. It must fail closed on unsupported or malformed evidence and keep receipts outside the synchronized repository tree. This result does not satisfy runtime, scene or complete-recovery gates. Do not use WSL or Docker for this Windows-local workflow.
