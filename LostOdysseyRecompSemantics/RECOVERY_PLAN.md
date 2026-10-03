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

The route is whole-program and staged: establish address/source identity at the boundary, recover types and ABI, prioritize leaf functions and their dependencies, assemble validated subsystems, build adapters for behavior comparison, validate representative scenes, then replace runtime boundaries one at a time. This checkpoint retains forty-three readable functions: guest memory copy/move, dynamic-array operations, special/raw allocation, manager initialization/construction, lock helpers and storage initialization, plus the earlier query, allocation, service, heap and cache helpers. The manager-startup composition now adds six supplemental cases across ten original/recovered bodies; its image provenance and runtime boundaries remain limited. Some cases still use bounded stubs or synthetic combinations. The next depth is the actual vtable methods, CRT/native-kernel and unwind boundaries, ABI adapters and representative runtime scenes; the free-return ABI, manager lifetime/concurrency and complete object contracts remain unresolved. The cache trace comparison does not establish hardware synchronization behavior. Generated PPC remains the baseline.

## Acceptance

A function is eligible for default runtime replacement when its identity, types, control flow, side effects and dependencies are documented; its bounded behavior comparison has a reproducible receipt; its runtime adapter is checked; and its scene evidence covers the intended scope. The source and bounded comparisons can be reviewed before runtime acceptance. A passing static-library build alone does not satisfy these gates. Runtime replacement, full-game recovery, frame-rate claims and cross-platform acceptance require separate evidence.

The current progress report covers 62,627 candidate addresses, with forty-three readable and forty-three bounded-comparison functions and zero runtime, scene or complete functions. The forty-two ordinary PPC functions account for 21,398 cases; query-lifecycle adds 74 cases, the supplemental heap-lifecycle runner adds one multi-body Allocate-to-Free-to-Allocate sequence, manager lifecycle adds 12 cases, manager-lock composition adds 12 cases and manager-startup adds 6 cases, for 21,503 bounded comparison cases. The startup runner uses the hash-pinned private unpacked image by default or an explicit `--image` path, pins XEX and image separately and checks four slots; image decryption/unpacking provenance is not independently proved. Its ten bodies use real virtual targets, while heap, CRT and native-critical-section pieces remain synthetic. Generic ABI scratch and volatile context are excluded; runtime, scene, complete semantics, MMIO, fault and concurrency remain unverified. Native Windows Release static-library validation is reused for this documentation-only composition update. This result does not satisfy runtime, scene or complete-recovery gates; no hook is enabled. Do not use WSL or Docker for this Windows-local workflow.
