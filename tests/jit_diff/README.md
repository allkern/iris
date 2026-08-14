# EE/IOP recompiler differential test

Answers one question: **does this recompiler backend generate code that computes
the same thing as another backend's?**

One build runs a fixed set of cases through `run_block` and records what came
out. Another build, on another architecture, replays the same cases and
compares. The x86 backend is the reference because it is the one known to work.

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

Other options: `--ee` / `--iop` to run one core, `--pass NAME` to run one pass,
`--block-size N` to change how long the generated blocks get, `--signatures-out
PATH` to write one signature per line for `comm(1)`, `JITDIFF_TRACE=1` to print
each case - disassembled - before it runs, which is how you find the block if a
case hangs or crashes.

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
[ee/ee-block] 4000 cases, 39 divergences in 5 signature(s) (seed=0xa486e0c22c0e684a)
  ee block[14] gpr.lo   x13   case=761 tag=4f227ef6 words=14 arm64=...4000 x86_64=...e462
```

## Passes

A run walks a fixed list of passes, and record and compare have to walk the same
list in the same order. Each pass has its own seed, so editing one does not
shift the cases every later pass generates.

| pass | core | shape | fastmem |
| ---- | ---- | ----- | ------- |
| `iop-single` | IOP | one instruction | n/a |
| `iop-block` | IOP | a block | n/a |
| `ee-single` | EE | one instruction | off |
| `ee-single-fastmem` | EE | one instruction | on |
| `ee-block` | EE | a block | off |
| `ee-block-fastmem` | EE | a block | on |
| `ee-except` | EE | one instruction that leaves | off |

### Shapes

**Single** is one instruction, a nop, and a jump back to the start. Every
sub-block holds one instruction, so the register cache is allocated and flushed
around each instruction on its own. It answers "does this instruction compute the
right value".

**Block** is a run of instructions - length varies per case - with the branches
among them aimed at slots inside the run, and a jump back to the start at the
end. This is the shape real guest code has, and the only one that puts the
register cache, constant folding, register pressure and sub-block chaining under
any load. A single-instruction case never makes the allocator spill and never
builds an edge between two sub-blocks.

Three registers are held out of the operand pool so the generator can rely on
them: one holds a scratch address, one is re-materialised by a `LUI` immediately
before each memory instruction (which is what puts a compile-time-known address
in the register cache and reaches the constant-folded pointer path), and one
holds a code address for `JR`/`JALR`. Every memory instruction is steered at a
16 KB scratch window with the base in the middle of it, so both positive and
negative displacements are legal and nothing a case does can reach the code it
is running or the next case's state.

**Except** is the set the other two shapes leave out because it does not stay
inside the case: `SYSCALL`, `BREAK`, `ERET` and the traps. The BIOS spends most
of its time in exactly these - a syscall out and an `ERET` back - and `ERET` is
twenty-odd emitted instructions with two branches and a call in the middle.
`EPC`, `ErrorEPC`, `EXL`, `ERL` and `fmv_skip` are all seeded both ways so every
arm of it is reached.

### Fastmem

`ee-single-fastmem` and `ee-block-fastmem` share a seed group with their
slow-path twins, so the two run the same cases. The only difference is whether
the bus fastmem tables are populated, which decides whether a load or store goes
through the inline sequence or calls out to the bus.

Fastmem is meant to be transparent, so **the two recordings have to come out
byte for byte identical**:

```
iris-jit-diff --pass ee-block         --record slow.rec
iris-jit-diff --pass ee-block-fastmem --record fast.rec
cmp slow.rec fast.rec
```

That is an oracle that needs no second architecture, and CI runs it on the native
slice. It matters because shipped builds always run with fastmem on, and the
harness this replaces left the tables zeroed - so the inline load/store sequence
and the constant-address folding path were emitted on every build and never once
executed.

## In CI

`.github/workflows/macos.yml` builds the universal binary, records with the
x86_64 slice and compares with the arm64 slice. **Any** divergence fails the job
- in this mode there is no known-noise to tolerate, a disagreement means one
backend miscompiles. The fastmem cross-check above runs first, on the native
slice.

The x86_64 slice needs Rosetta. The workflow probes for it and falls back to
`--interpreter` on the native slice if it is missing, which still catches the
fatal-error class.

## Known scope limits

- **VU0 macro mode (COP2 with the CO bit set) is skipped**, and so are the two
  `CTC2` encodings that start a microprogram: writing VI31 (CMSAR1) starts VU1,
  and the interlocking form starts VU0. Neither VU has a program loaded here, so
  what runs is a page of zeroes with no E bit in it and the core never comes
  back. `vu_cached.cpp` is a second recompiler and still has no harness at all.
- **The TLB group is out of scope.** `TLBWI` and `TLBWR` edit the page table,
  which outlives the case that ran them, and a case has to start from the state
  its seed describes. Memory instructions are steered into KSEG0 with a real base
  register so nothing takes a TLB miss either.
- **Eight of the twelve EE trap instructions are not implemented in the core** -
  `TGEI`, `TGEIU`, `TGEU`, `TLT`, `TLTI`, `TLTIU`, `TLTU`, `TNEI` each log a
  fatal error and leave the block uncompiled. They are left out of `ee-except`
  so they do not bury every other result; `TGE`, `TEQ`, `TNE` and `TEQI` are in.
- **Overflow traps are kept out of the block shape.** They leave for the
  exception vector and nothing after them in the case runs, which costs more
  coverage than the trap itself is worth there. The single shape seeds them
  narrow so the trap never fires, and `ee-except` covers the exception path.
- **Interrupts are not covered.** Both cores are seeded with interrupts masked,
  because an IRQ taken mid-case would desynchronise the two runs for reasons
  that have nothing to do with codegen.
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

It is also a weaker oracle in the block shape than in the single shape: it
replays `steps` interpreter steps against one recompiled block, and once a block
has edges in it the two do not retire the same instructions in the same order.
