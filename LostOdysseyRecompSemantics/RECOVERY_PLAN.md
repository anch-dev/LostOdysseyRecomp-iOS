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

The route is whole-program and staged: establish address/source identity at the boundary, deduplicate catalog candidates without erasing address identities, audit entry and body boundaries, then recover types and ABI, prioritize leaf functions and their dependencies, assemble validated subsystems, build adapters for behavior comparison, validate representative scenes, and replace runtime boundaries one at a time. This checkpoint retains forty-nine readable functions, including manager allocation/resize and fallback resize, alongside the earlier query, allocation, service, heap, cache, memory and manager helpers. Manager-startup adds six supplemental cases, manager-allocation and fallback-resize add 12 each; their image provenance and runtime boundaries remain limited. Some cases still use bounded stubs or synthetic combinations. Exact-body families are evidence for implementation reuse only: keep every entry address mapped and independently compared. The next priority is the function-inventory boundary audit before expanding recovery; existing recovered functions remain unchanged. Later work covers deeper vtable methods, CRT/native-kernel and unwind boundaries, ABI adapters and representative runtime scenes. The free-return ABI, manager lifetime/concurrency and complete object contracts remain unresolved. The cache trace comparison does not establish hardware synchronization behavior. Generated PPC remains the baseline.

## Acceptance

A function is eligible for default runtime replacement when its identity, types, control flow, side effects and dependencies are documented; its bounded behavior comparison has a reproducible receipt; its runtime adapter is checked; and its scene evidence covers the intended scope. The source and bounded comparisons can be reviewed before runtime acceptance. A passing static-library build alone does not satisfy these gates. Runtime replacement, full-game recovery, frame-rate claims and cross-platform acceptance require separate evidence.

The current progress report covers 62,627 candidate addresses, with forty-nine readable and forty-nine bounded-comparison functions and zero runtime, scene or complete functions. The forty-eight ordinary PPC functions account for 21,536 cases; query-lifecycle adds 74 cases, the supplemental heap-lifecycle runner adds one multi-body Allocate-to-Free-to-Allocate sequence, manager lifecycle adds 12 cases, manager-lock composition adds 12 cases, manager-startup adds 6 cases, manager-allocation composition adds 12 cases and fallback-resize composition adds 12 cases, for 21,665 bounded comparison cases. The six new functions have individual receipts; the two new composition receipts are supplemental and excluded from the per-function list. The startup runner uses the hash-pinned private unpacked image by default or an explicit `--image` path, pins XEX and image separately and checks four slots; image decryption/unpacking provenance is not independently proved. Its ten bodies use real virtual targets, while heap, CRT and native-critical-section pieces remain synthetic. Generic ABI scratch and volatile context are excluded; runtime, scene, complete semantics, MMIO, fault and concurrency remain unverified. The native Windows Release static library passed with all three new modules in semantic-recovery-isolated-library. This result does not satisfy runtime, scene or complete-recovery gates; no hook is enabled. Do not use WSL or Docker for this Windows-local workflow.
