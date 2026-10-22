# Vanilla 1.13c Testing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the approved vanilla 1.13c compatibility and release-testing program in independently reviewable packages.

**Architecture:** Portable tests replay identified original-game results. Local corpus and live-game lanes supply independent compatibility evidence. One qualification report binds every required result to the candidate source, executable, and packages.

**Tech Stack:** Existing C, Unity, CMake/CTest, Python standard-library unittest, Pillow, Ghidra/REA evidence, and the existing d2-fleet invocation infrastructure.

**Spec:** [Approved design](../specs/2026-10-21-vanilla-113c-testing-design.md).

## Global Constraints

- Vanilla Diablo II 1.13c is the sole game compatibility target; include its historical DS1 formats.
- Keep Windows x86, Linux x86_64, and macOS arm64 portable/package qualification.
- Portable CI requires no game install, original DLLs/MPQs, fleet, or REA.
- Original references are identified by hash; never use the mixed local tile tree as the vanilla baseline.
- Required unavailable, empty, timed-out, stale, or partial comparisons cannot qualify a release.
- Keep bulk game data local; public fixtures are authored inputs and small structured results.
- Preserve existing work and the index. Do not commit unrelated files, discard edits, or change sibling repositories.
- No release tag, publication, or version bump is part of executing these plans.

## Review Focus

1. False-positive results: A tasks 1–3 and F task 3 exercise empty, skipped, crashed, and stale runs.
2. Reference contamination: B tasks 1–3 and F task 3 exercise wrong DLL/MPQ/contract/source identities.
3. Data loss: C tasks 2–4 exercise failed write/close/publish, actual undo, and save-through-game checks.
4. Plausible wrong pictures: D tasks 1–3 exercise channel order, origins, alpha, frame order, and wrong-model controls.
5. Incomplete distribution: F tasks 1–3 exercise archive traversal, missing runtime files, native entry points, and evidence binding.

## Work package plans

| Package | Plan | Dependencies |
| --- | --- | --- |
| A | [Trustworthy runner results](2026-10-21-testing-a-results.md) | None |
| B | [Reference identity and equivalence pilot](2026-10-21-testing-b-equivalence.md) | A |
| C | [Map, editing, and persistence integrity](2026-10-21-testing-c-map-integrity.md) | A; B for reference comparisons |
| D | [Visual and export correctness](2026-10-21-testing-d-visuals.md) | B; C for map truth |
| E | [Robustness and performance](2026-10-21-testing-e-robustness.md) | C/D adapters |
| F | [Native distribution and qualification](2026-10-21-testing-f-release.md) | A; consumes B–E evidence |

Execute A then B before expanding native reference adapters. C's authored tests
and F's package tests can proceed independently once their inputs are stable.
Do not classify any later package as complete merely because its portable half
passes. A reference route that cannot observe the required state is an explicit
unavailable dependency, and the affected release requirement remains blocked.

## Start-of-execution preflight

- [ ] Confirm the selected execution method and workspace preference. The current checkout is the main working tree with substantial existing changes; an isolated worktree requires consent under the worktree skill.
- [ ] Capture `git status`, the staged/unstaged binary diffs, and hashes of relevant untracked source/tests. Keep runtime caches, credentials, IDE files, and nested worktrees out of transfer/commit sets.
- [ ] If isolating, reproduce the approved working source state there, including current untracked selection implementation/tests; a bare checkout of HEAD is not the previously reviewed baseline. Verify the copied source identities before edits.
- [ ] Run the chosen configured build/CTest baseline; preserve logs and explain environmental failures separately. The historical 29/29 result is not a fresh baseline for a new worktree.
- [ ] During each task, run its failing regression first, implement only the supported fix, and run the named checks. Commit only task-owned changes once reviewed; never blanket-stage this checkout.

## Shared contracts

Package B defines the canonical `CaseResult`, comparison contract, baseline, and
capture schemas. C–F consume those exact interfaces. If B's native pilot needs
an adjustment, update its interface block and downstream references together
before implementation continues. Schema decisions live in this repository;
existing decomp outcome names remain bounded to their measured domains.

Every later plan below identifies concrete cases and adapter commands. Native
ABI/state recipes are derived in its explicit reference-feasibility task and
stored with evidence before calls. Do not invent hidden register/global state
in order to make a prose plan look executable.

## Spec coverage and finish checks

- [ ] A covers result integrity, golden-update separation, and failure artifacts (design sections 2, 8).
- [ ] B covers corpus/reference identity, provenance, independent comparisons, negative controls, and the ownership seam (sections 3–6).
- [ ] C and D cover every game/editor behavior row, including installation/resources and project save paths (section 7).
- [ ] E covers generated invalid inputs, sanitizer/resource workloads, coverage disclosure, and measured performance (sections 7, 11).
- [ ] F binds all ten approved release requirements to candidate artifacts and includes native workflows/documentation (sections 10–11).
- [ ] All required case IDs appear in the behavior ledger. No row is labelled complete solely from a source review, byte-match status, or an empty run.
- [ ] The qualification report preserves remaining unknowns and optional sweep coverage; no automatic golden refresh or release publication occurs.

## Execution handoff

The design is approved. These implementation plans are ready for maintainer
review and selection of execution method. Native execution is recommended for
the first two packages because result/provenance interfaces are tightly coupled;
subagent-driven execution offers independent per-task review at greater cost.
