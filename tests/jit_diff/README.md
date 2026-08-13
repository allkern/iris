# EE/IOP recompiler differential test

Answers one question: **does this recompiler backend generate code that computes
the same thing as another backend's?**

One build runs a fixed set of single-instruction cases through `run_block` and
records what came out. Another build, on another architecture, replays the same
cases and compares. The x86 backend is the reference because it is the one known
to work.

Nothing in that loop involves an interpreter. There is an `--interpreter` mode,
but it is a fallback for when only one machine is available - see the caveats at
the bottom.

## Building

```
cmake -S . -B build -DIRIS_BUILD_TESTS=ON
cmake --build build --target iris-jit-diff
```

On macOS the binary is universal, so both backends come out of one build.

## Running

```
iris-jit-diff --record  jit-x86_64.rec --label x86_64
iris-jit-diff --compare jit-x86_64.rec --label arm64 --other-label x86_64
```

with the **same `--seed` and `--iterations` on both sides** - they have to
generate the same cases in the same order for the recording to line up. A
mismatch is detected and reported rather than silently compared against the
wrong case.

On a macOS universal binary, both runs happen on one machine:

```
arch -x86_64 ./build/tests/jit_diff/iris-jit-diff --record jit-x86_64.rec --label x86_64
arch -arm64  ./build/tests/jit_diff/iris-jit-diff --compare jit-x86_64.rec --label arm64 --other-label x86_64
```

Other options: `--ee` / `--iop` to run one core, `--signatures-out PATH` to write
one signature per line for `comm(1)`, `JITDIFF_TRACE=1` to print each case
before it runs (how you find the opcode if a case hangs or crashes).

Exit status:

| code | meaning |
| ---- | ------- |
| 0 | clean |
| 1 | divergences |
| 2 | a core logged a fatal error - failed codegen or an unimplemented opcode |
| 3 | bad usage |

2 is the serious one. A block the recompiler cannot encode logs a fatal error
and leaves `block->func` null, and the core then makes no forward progress at
all. It presents as a freeze, not as a wrong value. The offending guest block is
logged just above the fatal error.

Output is grouped by signature rather than listing every case:

```
[ee] 3994 cases, 39 divergences in 5 signature(s) (seed=0xa486e0c22c0e684a)
  ee lwl gpr.lo   x13   op=885e1d06 [lwl $fp, 7430($v0)] steps=4 arm64=...4000 x86_64=...e462
```

## In CI

`.github/workflows/macos.yml` builds the universal binary, records with the
x86_64 slice and compares with the arm64 slice. **Any** divergence fails the job
- in this mode there is no known-noise to tolerate, a disagreement means one
backend miscompiles.

The x86_64 slice needs Rosetta. The workflow probes for it and falls back to
`--interpreter` on the native slice if it is missing, which still catches the
fatal-error class.

## Known scope limits

- **One instruction per block**, so the register cache, constant folding and
  block chaining are only lightly exercised.
- **VU0 macro mode (COP2 with the CO bit set) is skipped.** `VCALLMS` would run
  VU0 microcode with no E bit and hang. `vu_cached.cpp` is a second recompiler
  and has no harness at all.
- **Exceptions are out of scope.** Trap instructions, `syscall`, `break`, `eret`
  and the TLB ops are excluded; trapping arithmetic is seeded narrow so overflow
  never fires; memory ops are steered to KSEG0 with a real base register so they
  never take a TLB miss.
- **The fastmem path is not covered.** The test's bus hands out a zeroed
  `ee::bus::Bus`, so every `vfast` lookup misses and the slow callbacks are used.
- **Bugs both backends share are invisible.** That is the cost of using one
  backend as the reference instead of an independent model. The EE `SRL`
  sign-extension bug was found in interpreter mode and would not have shown up
  here.

## Why not the interpreter

`--interpreter` compares against `ee::step` / `iop::cycle` in the same process.
It needs no second machine, and it can catch bugs both backends share. But the
interpreters stopped being maintained when the emulator switched to the
recompiler, so a divergence is as likely to be an interpreter bug, and some of
its false positives are architecture-specific, which is exactly where it is
least useful:

- `MTC0` - the interpreters mask hardwired bits, the recompilers store raw.
- `syscall` - the two engines keep `pc` on a different convention within a
  block, so `EPC` differs by 4.
- `madda.s` / `msuba.s` on arm64 - the interpreter computes `acc += fs * ft` in
  one statement, and clang contracts that to a fused multiply-add on AArch64
  (one rounding) but not on x86-64 baseline (two roundings). The recompiler
  emits a separate multiply and add on both. So the interpreter is the one that
  changes behaviour between architectures, not the recompiler.
- MMI ops - the interpreter has an SSE implementation and a scalar one, chosen
  by `_EE_USE_INTRINSICS`, and they do not always agree with each other. The
  test target deliberately builds without it so the scalar path is used
  everywhere; `-DIRIS_JIT_DIFF_INTRINSICS=ON` switches to the SSE path that
  shipped x86 builds actually use.
