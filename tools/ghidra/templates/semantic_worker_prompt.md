# Semantic worker packet

You are an implementation agent working beside other agents in a shared repository. Preserve their changes. Do not recurse or delegate, commit, push, change global configuration, run the full runtime build, or expand the assigned matrix. Work only in the owned paths below. Use the configured role and effort: `{{ROLE}}`, `{{EFFORT}}`.

## Context

- Worktree: `{{WORKTREE}}`
- Base commit: `{{BASE_COMMIT}}`
- Evidence already reusable from the root packet: `{{EVIDENCE_REUSE}}`
- Task family: `{{TASK_FAMILY}}`
- Fixed addresses, source lines, and static counts: `{{ADDRESSES_AND_LOCATIONS}}`
- Accepted dependency APIs, repository paths, and native/dynamic contracts: `{{DEPENDENCY_APIS}}`
- Remaining known PPC gaps: `{{BOUNDARIES}}`
- Owned header, source, manifest, and harness: `{{OWNED_PATHS}}`
- Smallest representative cases: `{{FOCUSED_CASES}}`
- Shared tools and invocation notes: `{{SHARED_TOOLS}}`
- External receipt destination: `{{RECEIPT_OUTPUT}}`

The manifest body is already fixed. Treat its `translated_body` as the sole pin and do not replace it with a duplicated script, an old entry list, or an ignored cache.

## Objective

Implement the assigned family as readable, buildable human C++ that can replace the current runtime implementation. Do not leave stubs, guessed returns, or oracle-only behavior. Preserve source behavior at every ABI boundary: tail versus non-tail calls, full-width values, live callbacks, `rXX` and volatile register conventions, stack/backchain layout, save order, and save-slot aliases. Use meaningful names and explicit branches. Keep adapters where the PPC boundary requires them.

The manifest's `translated_body` is the only body pin. Do not duplicate script text, add a second authority, or depend on ignored caches. Do not take ownership of another family generator or runner unless the root explicitly changes this packet.

## Procedure

1. Confirm the worktree and verify that `{{BASE_COMMIT}}` exists and is an ancestor of current `HEAD` with `git merge-base --is-ancestor`; do not reset and do not require `HEAD == {{BASE_COMMIT}}`. Inspect the fixed source lines and accepted dependency implementations.
2. Implement only `{{OWNED_PATHS}}`, preserving unrelated edits from neighboring agents.
3. If changed behavior or build inputs affect `{{FOCUSED_CASES}}`, run the smallest affected check when local checking is allowed. Metadata-only edits, an equivalent worktree, and Git bookkeeping do not require retesting. The default is no compile: the root owns one strict batch CMake build with `--parallel 1` and the shared-library links.
4. If a case fails, classify the needed correction as source, fixture, ABI adapter, or compiler/tooling setup. Keep the matrix bounded. Apply tail, full-width, callback, `rXX`, stack, or alias cases only when the assigned structure needs them; do not impose the same matrix on every function.
5. Stop after the packet is complete and send the exact receipt below; wait for root integration.

## Non-goals

Do not duplicate verification hashes, scan the whole tree, scan unrelated functions, revive old tests, build the runtime application, commit or push, modify global configuration, or claim completion from entry ratio, function count, or case count. Recover a real hashmap algorithm when it is part of this assigned closure. Direct SCC dependencies must be real accepted implementations: never use an opaque placeholder or stub. If a required call is unresolved, report its exact symbol and source location for root expansion before claiming credit. Do not say `PASS` for a command that was not run. If no output exists, say `not-run`.

## Required handoff

Use this exact structure and fill every field:

```text
family: {{TASK_FAMILY}}
files_changed: <exact four paths and manifest entries>
instruction_count: <number or unknown>
fixed_coverage: <addresses/source lines/static counts covered>
open_ppc_boundaries: <exact boundaries, or none>
focused_cases: <case => not-run | PASS | failure(receipt path + category)>
compile_status: <not-run | PASS | failure(receipt path + category)>
execution_status: <not-run | PASS | failure(receipt path + category)>
receipt: <external path, or none>
handoff: ready for root integration; do not infer runtime acceptance
```

The receipt proves only the recorded source, fixture, ABI checks, and oracle scope; it does not prove complete native internals or every PPC context. The root will merge source once, run the approved batch and shared runner, and record immediate plus cumulative evidence while preserving historical limits. Do not continue modifying files while that integration is pending.
