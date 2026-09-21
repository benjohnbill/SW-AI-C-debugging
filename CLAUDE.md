# CLAUDE.md — SW-AI-C-debugging (week 5 memory-debugging lab)

The tutor spine is imported by the parent `CLAUDE.md`; this file fills its
slots.

## Purpose

Twenty C programs under `challenges/NN_name/bug.c`, each with exactly one
planted memory bug that crashes (SIGSEGV / SIGABRT / SIGBUS). The exercise is
the loop: crash → `bt` → locate the line → explain the cause → fix → confirm
exit 0. This is the second half of the set that started in week 4
(`benjohnbill/SW-AI-C`); what must remain is the ability to read a crash
backwards to its cause with gdb, and to reason about heap and stack ownership.

## Slot 1 — step 2 and the ladder

Step 2 is **draw the memory**: heap block lifetimes (alloc → alias → free →
use) or the stack frame layout. For realloc bugs: which pointers still hold the
old address. For overflows: the buffer size versus the index or length.

The other steps in this week's terms:

1. Predict before running: read `[시나리오]`/`[기대 동작]` and say where he
   expects it to die and with which signal.
2. Crash and locate: `make g_<name>` → `run` → `bt`. If the top frame is libc
   (`free`, `__stack_chk_fail`, `malloc_printerr`), `up` until his own file.
   "Where did it die" is a direct request; "which of my lines put the memory
   in that state" is convergent, even after he ran `bt`.
3. Confirm with evidence: `print`, `info locals`, `x/`, `watch`, or an
   `fprintf(stderr, …)` trail.
4. Fix and verify: `make r_<name>` exits 0 on the host, then `make dr_<name>`
   exits 0 in the container. Then: did the fix remove the cause or only the
   symptom (NULL-checking instead of fixing ownership)?

After it passes, one at a time, in order:

1. Which invariant was broken (ownership, lifetime, bounds), in one sentence.
2. Where else in the same file the same pattern appears and is still safe — why.
3. Would ASan or valgrind have caught it earlier, and what would the report
   say? Let him predict before running (`-fsanitize=address` is not in the
   Makefile; he adds it by hand if he wants it).

## Slot 2 — statement and spoilers

`[시나리오]` and `[기대 동작]` in the `bug.c` header are the problem statement.
`[증상]`, `[gdb 로 잡기]`, `[printf(로그)로 잡기]` and the `TODO` that states
the fix are spoilers; the README recommends attempting without them.

## Slot 3 — environment

Editor is **Zed** (Windows → WSL remote).

Host: Ubuntu 26.04 · gcc 15 · glibc 2.43 · gdb 17 with his own
`~/.config/gdb/gdbinit` + `~/.config/gdb/*.py` (repo `benjohnbill/gdb-config`).
`~/.gdbinit` is inert: six comment lines, no definitions.
Container (`memdbg`): Ubuntu 24.04 · gcc 13 · glibc 2.39 · gdb 15, with the same
`~/.config/gdb` bind-mounted read-only, so the commands below
work there too; only the gdb-16 color slots are skipped. Measured 2026-09-15:
18/20 crash identically on both.

He wrote every command in this table himself. A question about one is a direct
request, not the exercise. `help track` and friends are his path to the grammar,
not yours: you cannot run them. The definitions are on disk in
`~/.config/gdb/*.py`. Before you describe any grammar, flag, or state lifecycle
below, read `~/.claude/docs/tool-guides/gdb-commands.md` — it routes to the
defining file and lists what stock-gdb reasoning gets wrong here.

| Command | What it does |
|---|---|
| `dbg` · `vars` · `out` | TUI layouts: tall source · source and values · the program's own output (`out send TEXT`, `out off`) |
| `tk EXPR` · `tk -l EXPR` · `tk /FMT EXPR` | one row per expression, redrawn in place, not scrolled. `-l` pins the row to the address, so it survives leaving the frame — and is never re-pinned, so it outlives `rerun` pointing at whoever owns that address now. `/FMT` makes a memory row, compared word by word |
| `itk` · `utk N` · `utk 1..4` | list the rows · remove row N · remove rows 1 to 4, both ends included |
| `tk walk EXPR N [FIELD]` | the first N nodes of a chain, one row each. FIELD only when more than one field points at the node's own type |
| `tk deep EXPR N` | EXPR and every struct it reaches within N levels, one row each. N counts levels, so a tree branches fast |
| `tk tri[0..5]` · `tk each PATTERN` | one row per element or member. Tokens: `[A..B]`, `[A..]`, `[..B]`, `[..]`, `[]`, `.*`, `->*`. A bound is an expression: `tri[0..rows-1]`. No token steps an array or a struct whole |
| `-l` on all three | `tk walk -l` · `tk deep -l` · `tk each -l` pin every row of the group to its own address. The standalone `walk`, `deep` and `each` take no flag |
| `walk EXPR [FIELD]` · `deep EXPR N` · `each [/FMT] PATTERN` | the same three expansions, printed once instead of tracked. `each` and `deep` number every line `$N`, as `print` numbers its own, so `$3` reads one back. `walk` does not number its lines. `each` takes one format letter and refuses `/4xg` |
| `snap [LABEL]` · `snaps` · `back [LABEL]` | named checkpoints. `back` re-takes the flag it returns to, so the same one works again |
| `rebuild [TARGET]` · `rerun` | make the loaded binary · rebuild, kill the process, drop the source cache (a bare `directory`), run. A failed build stops before the kill |

A row shows `*` and `old -> new` when the value moved since the previous stop,
and `?` when the expression cannot be read from the selected frame.

- `10_realloc_dangling` does **not** crash on the host (glibc 2.43 misses the
  double free) and `17_ownership_uaf` aborts on host but segfaults in the
  container. Those two go through `make dr_…` / `make dg_…`.
- Final exit-0 verification of any fix runs in the container (`make dr_…`);
  that is the coach's reference environment. From a script, `DOCKER_TTY=-i`.

`GNUmakefile` is ours and wraps the upstream `Makefile` without editing it:

```
make <name> | r_<name> | g_<name>    host build | run | gdb (tab-completes)
make dcheck                          container: all 20 → crash summary
make dr_<name> | dg_<name>           container run | gdb
make dshell | dimage | dclean
```

Upstream `make run NAME=… / make gdb NAME=… / make check` still work.
`build/` is host output, `build-docker/` is container output; both ignored.

`lab/` is ours too: hand-written experiments and their write-ups.
`lab/heap-anatomy.md` is the glibc chunk reference — layout, the size formula,
reuse policy, and header-first debugging. Every claim in it was measured on both
environments (2026-09-21). `lab/hwalk.gdb` defines `hsize`, `hwalk` and `hwatch`
from it; section 10 is the drill that uses them.

Repo layout: `upstream` = `krafton-jungle/debugging_lab_docker` (pull only),
`origin` = `benjohnbill/SW-AI-C-debugging`. Sync with
`git pull upstream master`.

Verify: `make r_<name>` on the host, then `make dr_<name> DOCKER_TTY=-i`;
both exit 0.

Board: 4

## Slot 4 — do not touch

Never edit upstream-tracked files; add files. `.vscode/` and `.devcontainer/`
are upstream: leave them, never recommend F5 or Reopen in Container.
