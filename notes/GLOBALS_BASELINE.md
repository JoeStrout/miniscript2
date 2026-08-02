# Globals rework: performance baseline

Measurements taken **before** any of the work in [GLOBALS.md](GLOBALS.md), so
that each stage can be checked against them.  Re-run and append a section after
every stage; do not overwrite this one.

## How to reproduce

```bash
tools/benchmark.sh -lang=cpp-goto
tools/benchmark.sh -lang=cs,python,lua
```

## Environment

| | |
|---|---|
| Date | 2026-08-01 |
| Repo | `main` @ `7e1d828`, working tree dirty (the C#/generated changes already in progress) |
| CPU | Intel Core i9-9880H @ 2.30 GHz |
| OS | macOS 13.6.4 |
| C++ | Apple clang 15.0.0, `-O3 -DNDEBUG`, computed-goto dispatch |
| .NET | 10.0.301 |
| Python | 3.12.8 |
| Lua | 5.4.3 |

`cpp-switch` was not measured: dispatch mechanism is orthogonal to globals, and
skipping it avoids two full rebuilds.  Add it if a stage's result looks
dispatch-dependent.

## Results

Times are wall clock, one run each — good to maybe ±5%, so treat differences
under ~10% as noise.

| Benchmark | goto asm | goto src | C# asm | C# src | Python | Lua |
|---|---|---|---|---|---|---|
| Iterative Factorial | 0.373s | 1.426s | 3.016s | 8.120s | 2.385s | 0.651s |
| Iterative Fibonacci | 0.664s | 6.869s | 4.473s | 17.865s | 2.654s | 0.749s |
| Recursive Fibonacci | 1.812s | 4.457s | 5.072s | 10.652s | 0.709s | 0.374s |
| **Global Loop** | – | **1.112s** | – | **8.365s** | **3.221s** | **0.997s** |
| **Global Loop (locals)** | – | **1.124s** | – | **8.169s** | **1.281s** | **0.294s** |
| **Global Churn** | – | **1.895s** | – | **5.728s** | **0.247s** | **0.078s** |

The three globals benchmarks are source-only; see `tools/benchmarks/*.ms` for
what each measures and why.

## The numbers that matter

### 1. Top-level access costs nothing today — and that is what is at risk

`Global Loop` and `Global Loop (locals)` are the same loop, once at top level
and once inside a function.  Today MiniScript 2 puts both in registers, so:

| | MS2 (goto) | MS2 (C#) | Python | Lua |
|---|---|---|---|---|
| globals ÷ locals | **0.99x** | **1.02x** | 2.51x | 3.39x |

MS2 has no globals penalty at all; Python pays 2.5x and Lua 3.4x for the same
loop.  That is the cost of the "model-only" step in GLOBALS.md §4.3 if globals
become a plain hash map — Python and Lua bracket where we would land.

**This is the argument for the slot-indexed step (§4.3).**  A guarded slot
index should come in far under those ratios, because it is an array index, not
a hash lookup.  If stage 3 lands near 2.5x and stage 4 does not pull it back
under roughly 1.3x, the design is not paying for itself and we should say so.

### 2. Reaching globals by name from depth is pathological today

`Global Churn` never touches a global through a top-level named variable — it
goes through `globals[k]` and free identifiers from inside function frames.
Against the identical loop on an **ordinary map**, measured separately:

| 640,000 get+set pairs (goto build) | ordinary map | `globals` | ratio |
|---|---|---|---|
| as first measured | 0.12s | 4.01s | 33x |
| after the string fix (see Caveats) | 0.12s | 1.16s | **9.7x** |

Use the second row.  Fixing the string-representation bug took the globals path
from 4.01s to 1.16s — the keys are short, so they stopped being heap-allocated —
while the ordinary map was unaffected.  The pathology is smaller than it first
appeared but still an order of magnitude.

Every access pays two linear `VarMapBacking.FindOrderIdx` scans — one on the
get, one on the set — before reaching the hash table, and the scan is over
every register-bound global in the program.  Note also that Python and Lua run
this benchmark 8–24x faster than MS2 does, while losing to MS2 on the other
loops: this is a specific pathology, not general slowness.

**This is the clearest win available.**  Slot indexing makes it a bounded array
index. Expect a large improvement here; if stage 3 alone does not already move
it substantially, something is wrong.

### 3. Watch for collateral damage

The three pre-existing benchmarks are the control. `Recursive Fibonacci` in
particular is all function-local registers and should be untouched by any of
this; if it moves, the change has leaked into the non-global path.

## Caveats

- **`tools/benchmarks/` is git-ignored.**  `.gitignore:96` has a bare
  `benchmarks` entry (added for a symlink to `miniscript-benchmarks/cmdline`),
  which matches this directory too.  The existing benchmark files are tracked
  because they predate that rule; **the new `global_*` files need
  `git add -f`** or they will silently not be committed.
- **The `.msa` benchmarks now print their own result.**  The host's
  "Result in r0:" trailer moved behind `--debug` in commit `bb56e5d`, and
  `--debug` traces every instruction (`cs/VM.cs:1136`), so it cannot be used on
  a timed run.  Each `@main` now ends with an explicit `print` of r0 via the
  intrinsic; the added instructions are outside every loop.
- **A C++ string bug was found while building these, and has since been
  fixed.**  Short strings had two representations that compared equal but hashed
  differently, so a runtime-built map key would not match the same key written
  as a literal.  The fix (`value_hash` now hashes every string by content, and
  `adopt_ss` canonicalizes short results to tiny strings) landed after these
  numbers were taken, and it **moves `Global Churn`**, whose 32 `dynN` keys are
  4-5 bytes and so are now immediates rather than heap allocations:

  | | before the fix | after |
  |---|---|---|
  | Global Churn (goto src) | 1.895s | **0.962s** |

  `Global Loop` and `Global Loop (locals)` are unaffected (no strings).  **Use
  0.962s as the Global Churn baseline** for comparing later stages; the table
  above is left as taken so the two effects stay separable.
