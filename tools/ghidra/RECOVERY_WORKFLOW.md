# Semantic Recovery Workflow

This guide defines the handoff between the root integrator and implementation agents recovering PPC functions into readable native C++. It is the operating contract for the shared recovery tool and worker packets. The commands below are implemented and locally verified; their receipts remain bounded evidence and do not establish runtime replacement or complete PPC recovery.

## Objective and boundaries

The target is a buildable, human-readable C++ implementation of the assigned function family that can replace the current runtime path. A stub, guessed return value, wrapper that bypasses the source behavior, or implementation that only satisfies an oracle is incomplete. Preserve the source behavior at the ABI boundary, including register and stack layout, tail calls, full-width values, live callbacks, `rXX` conventions, save ordering, and save-slot aliases.

The root agent groups work by dependency closure and strongly connected component (SCC). At most two implementation agents work in parallel. Native compiler concurrency is capped at two jobs, and the root batch always uses `--parallel 1`. Agents must not expand the dependency matrix, recursively delegate, change global configuration, commit, push, or run the full application build. They work only in their assigned paths and leave the integration build to the root.

Do not use a function count, entry ratio, or number of generated cases as a completion claim. A family is complete only when its semantics, ABI boundary, focused cases, and integration receipt support the stated scope. Duplicate verification hashes, whole-tree scans, old test suites, runtime builds, and unrelated families are non-goals for a focused packet. Recovering a real hashmap algorithm is allowed when it belongs to the assigned dependency closure.

## Root preparation

Before dispatch, the root records the base commit and creates a packet from current evidence. The packet must contain:

* the full objective: final human-readable native C++ replacement for the assigned family, with no stubs;
* `WORKTREE` and `BASE_COMMIT`, family name, fixed source locations, addresses, static counts, and the exact source lines that pin each body;
* dependency APIs already accepted, with repository paths and explicit native and dynamic contracts;
* remaining known PPC gaps and the reason each gap is outside this packet;
* the four owned paths: header, source, manifest, and harness. The manifest body is already fixed; a family generator or runner is not automatically owned;
* the smallest representative branch cases, including tail versus non-tail calls, full-width values, live callbacks, `rXX`, stack/backchain behavior, save order, and save-slot aliases where applicable;
* shared tool paths and the expected receipt destination outside the checkout;
* explicit non-goals and the instruction that the agent is working beside other agents and must preserve their changes.

The root approves the dependency closure before dispatch. If an agent finds a missing dependency, it reports the exact symbol, source location, and contract gap; it does not silently absorb another family.

## Worker procedure

1. Read the packet, inspect the pinned source lines and accepted dependency implementations, and confirm that `BASE_COMMIT` exists and is an ancestor of the current `HEAD` (`git merge-base --is-ancestor BASE_COMMIT HEAD`). Do not reset the worktree or require `HEAD == BASE_COMMIT`; current root and neighboring-agent commits may have advanced it. Do not rely on full conversation history or an unpinned generated file.
2. Derive meaningful names, branches, data flow, and ownership from the PPC body, callers, literals, and accepted contracts. Direct dependencies in the assigned SCC must be real accepted implementations; opaque placeholders or stubs do not close the dependency. If a required call remains unresolved, report the exact call and location so root can widen the closure before crediting the family. Other unrelated PPC gaps may be recorded as limitations. Keep adapters at register and stack boundaries. Preserve source behavior even when a shorter implementation would pass the current fixture.
3. Implement only the four owned files. Keep the manifest's `translated_body` as the sole pin for the recovered body; do not duplicate script text or use ignored caches as a second source of truth.
4. If changed behavior or build inputs affect a focused case, run the smallest affected check when the packet permits local checking. Metadata-only edits, an equivalent worktree, and Git bookkeeping do not require retesting. The default worker mode does not compile. The root owns the one strict batch CMake build and shared-library link; a worker must not report a pass from an unrun command.
5. Produce the completion handoff described below, then wait for root integration. A failed case must identify whether the required change is in source, fixture, ABI adapter, or compiler/tooling setup. Do not respond by expanding the case matrix.

## Shared batch tool

The public helper is `tools/ghidra/semantic_recovery.py`:

```text
semantic_recovery.py check --manifest <paths> [--ppc-root <path>]
semantic_recovery.py run --batch <root-relative JSON> --output <external scratch> \
  --library-build <configured CMake dir> --msvc-runtime MT|MD|MTd|MDd \
  [--library-file <exact .lib>] [--ppc-root <path>]
semantic_recovery.py progress [--runtime-wrappers <count>]
```

`check` validates complete pinned bodies and manifest metadata. It does not perform an automatic source-ownership audit; ownership is enforced by the packet and reviewed by the worker and root. `run` takes a root-relative batch JSON and writes receipts and logs only to the external output directory. With `--library-build`, the tool performs one strict incremental CMake build using `--parallel 1`, then links every oracle against the same library. `--msvc-runtime` is required and must match the configured `MT`, `MD`, `MTd`, or `MDd` runtime; `--library-file` names the exact library when discovery would be ambiguous. The shared helper paths [recovery_abi.h](../../LostOdysseyRecompSemantics/include/lo_semantics/recovery_abi.h) and [semantic_oracle_support.h](../../LostOdysseyRecompSemantics/tests/semantic_oracle_support.h) provide frame/register support, while each family defines its own native prelude and mapping.

The batch JSON design is:

```json
{
  "families": [
    {
      "name": "crt-status-error",
      "manifest": "LostOdysseyRecompSemantics/crt_status_error_families.json",
      "harness": "LostOdysseyRecompSemantics/tests/crt_status_error_oracle.cpp",
      "prelude": "// two-line realNative declaration/mapping, following recovery_batches/crt_stream_errors.json",
      "sources": ["LostOdysseyRecompSemantics/src/crt_status_error.cpp"]
    }
  ]
}
```

The manifest is a family manifest, not an old `recovery.json.functions` entry list. Existing families such as `crt-status-error` and `crt-stream-error` are packet examples only. The verified tool checks 11/11 small utility cases, and the shared two-header Windows `/W4 /WX` fixture; those utility checks are tool validation, not recovery-case credit. Current bounded batch evidence is two pending families, four entries, 13 PPC cases and two unknown entries; the strict library build passed and all oracles linked against the same `.lib`. The prior 13 standalone cases are reruns, not additional recovery credit. A single CRT stream observation measured 6.621 seconds for standalone source compilation and 1.266 seconds for shared-library linking; this local observation does not establish a general speedup multiplier.

`progress --runtime-wrappers 3168` counts mapping JSON committed at `HEAD` and reports the current ratio. For example, the current output reports `5,159/62,627` mapped addresses (`8.238%`) and `3,168` runtime wrappers. These are HEAD-local bookkeeping values, not a fixed total-progress claim; do not copy them into a new recovery completion claim.

## Receipt and integration handoff

Every worker handoff must state exactly:

* files and manifest entries changed;
* instruction count and fixed source/address coverage;
* open PPC/native boundaries and every known limitation;
* focused cases attempted and their outcome: `not-run`, `PASS`, or a failure receipt with path and category;
* whether compilation or execution was skipped, and why;
* the external receipt path, if one exists.

No output means no pass. Changed behavior or build inputs require the smallest affected check; metadata-only edits, an equivalent worktree, and Git bookkeeping do not require retesting. The root integrator merges the source once, performs one cumulative CMake/build step with `--parallel 1`, and runs each family focus through the shared runner. The integration record appends the changed delta, generated count, cumulative `62,627` context where relevant, coverage percentage, and runtime `3,168` context only when supported by the current evidence. It preserves all historical limitations and does not rewrite the long historical record. Path-scoped staging and immediate commits remain root responsibilities; each commit records its immediate and cumulative evidence.

A receipt proves only the recorded source, fixture, ABI checks, and oracle scope. It does not prove that native internals cover every PPC context, indirect edge, fault mode, concurrency mode, or runtime path. The tool does not scan the full tree, hash arbitrary caches, or run the full runtime.

## Review checklist

Before accepting a packet, the root confirms that the worker did not modify runtime code outside the four paths, duplicate manifest authority, hide an unresolved ABI boundary, claim an unrun test as passed, or treat entry ratio as completion. The root then reviews the documentation delta separately from implementation and build evidence.
