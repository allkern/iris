# EE/IOP recompiler differential test

Both cores ship two implementations of every instruction: the recompiler and the
interpreter (`ee::run_block` / `ee::step`, `iop::run_block` / `iop::cycle`). This
runs one instruction at a time through both from identical state and compares the
architectural result.

The interpreter is portable C++, so it computes the same answer on every host.
The recompiler goes through asmjit's `ujit`, which has a separate backend per
architecture. So: **a divergence that appears on one host and not another is a
codegen bug in that host's backend.** That is what this is for.

## Building

```
cmake -S . -B build -DIRIS_BUILD_TESTS=ON
cmake --build build --target iris-jit-diff
```

On a macOS universal build both slices are produced. Run each explicitly:

```
arch -x86_64 ./build/tests/jit_diff/iris-jit-diff
arch -arm64  ./build/tests/jit_diff/iris-jit-diff
```

## Running

```
iris-jit-diff [--seed N] [--iterations N] [--ee | --iop] [--signatures-out PATH]
```

The seed drives everything, so the same seed generates the same cases on every
machine. Keep the seed *and* the iteration count fixed when comparing hosts -
the set of signatures grows with the number of cases. `JITDIFF_TRACE=1` prints
each case before it runs, which is how you find the opcode responsible if a case
hangs or crashes.

`--signatures-out` writes one signature per line, sorted and nothing else, for
diffing two runs with `comm(1)`.

Exit status:

| code | meaning |
| ---- | ------- |
| 0 | clean |
| 1 | divergences |
| 2 | a core logged a fatal error - failed codegen or an unimplemented opcode |
| 3 | bad usage |

2 is the serious one. See the note on fatal errors below.

Output is grouped by signature rather than listing every case:

```
[ee] 4000 cases, 36 divergences in 4 signature(s) (seed=0xa486e0c22c0e684a)
  ee lwl gpr.lo    x12   op=89921000 [lwl $s2, 4096($t4)] steps=4 jit=...007fc040 interp=...00000000
```

To find backend-specific bugs, run the same seed and iteration count on both
architectures and diff the signature lists. Signatures present on both are
architecture-independent. **Signatures present on only one are that backend's
codegen bugs.**

The `N fatal error(s) logged by the cores` section matters as much as the
divergences: a recompiler that fails to encode an instruction logs a fatal error,
leaves `block->func` null, and the core then makes no forward progress at all. It
does not show up as a wrong value, it shows up as a freeze.

## In CI

`.github/workflows/macos.yml` builds the universal binary and runs **both
slices** with the same seed, then diffs their signature lists against each other.
That self-baselines: no checked-in expected-output file to keep up to date.

The job fails when a core logs a fatal error, or when the two slices disagree.
Divergences that appear in both slices do not fail it - those are the known
architecture-independent bugs, and gating on them would just keep CI red.

The x86_64 slice needs Rosetta. The workflow probes for it and carries on
without the comparison if it is missing, so the arm64 result is never lost to a
runner-image change.

## Known scope limits

- **The oracle is pinned to the scalar MMI interpreter** on every host
  (`_EE_USE_INTRINSICS` is deliberately not defined here) so that an x86 run and
  an arm64 run are comparable. `-DIRIS_JIT_DIFF_INTRINSICS=ON` switches the
  oracle to the SSE path that shipped x86 builds actually use; that is a
  different question and gives different results.
- **VU0 macro mode (COP2 with the CO bit set) is skipped.** The recompiler hands
  every one of those to the same interpreter routine, so there is no codegen to
  compare, and `VCALLMS` would run VU0 microcode with no E bit and hang.
  Validating `vu_cached.cpp`, which is a second recompiler, needs its own
  harness.
- **Exceptions are out of scope.** Trap instructions, `syscall`, `break`, `eret`
  and the TLB ops are excluded, and the operands for trapping arithmetic are
  seeded narrow so overflow never fires. An exception taken part way through a
  block is a real question, but an architecture-independent one.
- **The fastmem path is not covered.** The test's bus hands out a zeroed
  `ee::bus::Bus`, so every `vfast` lookup misses and both engines go through the
  slow callbacks. Covering the fast path needs a bus with real fastmem tables.
- One instruction per block, so the register cache and constant folding are only
  lightly exercised. Multi-instruction blocks would widen coverage.
