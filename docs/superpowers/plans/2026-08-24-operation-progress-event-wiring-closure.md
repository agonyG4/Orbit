# Operation Progress Event Wiring Closure

## Scope

Close the remaining Astrea Explorer operation-progress defects without changing the file-operation model, archive streaming behavior, or the existing presenter lifecycle policy.

## Implementation tasks

1. Make the native adapter publish aggregate file/archive snapshots from the real `AppStateFacade` signals.
2. Make `FileOperationsState` the public operation boundary and connect the presenter to that boundary only.
3. Reconcile every live snapshot, including numeric progress updates while semantic state remains `running`.
4. Keep fast terminal results pending until the minimum running-display interval expires, with generation-safe preemption.
5. Publish native file-operation terminal states once, coherently, with `running == false` in the same aggregate event.
6. Add bridge, presenter lifecycle, live-progress, preemption, and native signal-coherence regression coverage.
7. Document the event/snapshot contract and presentation-only minimum-display rule.

## Verification

- Reconfigure and build the release native target.
- Run focused operation-progress/controller/compatibility tests and the full CTest suite.
- Run the Explorer QML unittest suite and inspect the final diff for whitespace/errors.
