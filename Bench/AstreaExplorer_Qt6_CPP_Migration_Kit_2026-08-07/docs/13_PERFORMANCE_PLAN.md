# Performance and Resource Qualification

The migration is expected to improve architecture and responsiveness, but performance claims must be measured.

## Compare Three States

### A. Current Quickshell Explorer

Current production architecture.

### B. Native Qt 6 + one-shot Rust CLI

Measures the benefit of removing Quickshell/QML orchestration before backend persistence.

### C. Native Qt 6 + persistent Rust worker

Measures the final recommended architecture.

## Memory

For every resident process, collect `/proc/<pid>/smaps_rollup` and record:

```text
Rss
Pss
Pss_Anon
Pss_File
Private_Clean
Private_Dirty
Swap
```

Use combined PSS as the primary memory comparison, not summed RSS.

## CPU / Wakeups

Measure:

- idle CPU for a fixed quiet interval;
- process CPU time;
- thread counts;
- open FD counts;
- child-process spawn count;
- watcher count;
- repeated timer/wakeup behavior where observable.

## Latency

Measure at least 30 iterations where practical:

- cold Explorer startup to first usable frame;
- warm startup;
- load 100-entry directory;
- load 1,000-entry directory;
- load 10,000-entry directory;
- sort change;
- search first result / completion;
- tab switch;
- paste operation start acknowledgement;
- Alt-level UI responsiveness during a large file operation;
- FileChooser first visible frame.

Report median and p95.

## Spawn Reduction

Record how many external processes are started for:

1. normal Explorer startup;
2. one directory navigation;
3. one search;
4. one copy/paste;
5. 30 seconds idle.

The persistent worker phase should demonstrate a material reduction in backend process spawns.

## Success Direction

The goal is:

- lower or equivalent idle PSS;
- lower idle process/wakeup overhead;
- materially fewer process spawns;
- better large-directory responsiveness;
- no regression in visual frame pacing;
- no regression in FileChooser latency;
- no GUI-thread stalls caused by backend work.

Do not set an arbitrary RAM percentage target before measuring the real native build.
