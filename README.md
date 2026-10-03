# Life: an observable C arena experiment

A deliberately small Conway's Game of Life: a 40×20 wrapping board, a fixed
glider seed, and ncurses visualization. The allocator and its lifetime are the
experiment; the UI is not a feature-rich game.

## Build and run

Requires Linux, a C11 GCC or Clang compiler, Make, pkg-config, and ncurses
development headers/libraries. Common package names are libncurses-dev
(Debian/Ubuntu) or ncurses-devel (Fedora).

- `make` builds the debug application at [build/debug/life](build/debug/life).
- `make run` builds and launches it. Use a terminal at least 40 columns × 23 rows.
- `make release` builds the optimized application at [build/release/life](build/release/life).
- `make test` runs allocator and simulation tests in debug **and** release.
- `make check` also runs AddressSanitizer/UndefinedBehaviorSanitizer tests.
- `make sanitize` builds a sanitizer-instrumented interactive application.
- `make clean` removes generated build outputs.
- To change compiler, clean first, then use `make CC=clang check` (or `CC=gcc`).

VS Code tasks are provided for **Build and verify arena experiment** and
**Run Life**. Verification uses Clang because this environment's GCC sanitizer
runtime files are missing. GCC debug/release builds work; using GCC sanitizers
requires its ASan/UBSan runtime packages (commonly libasan and libubsan). After
installing them, either compiler can be used. The default compiler remains `cc`.
If compiler flags change, clean before rebuilding; output directories separate
the build modes, not arbitrary compiler/flag combinations.

The simulation starts paused. **Space** pauses/resumes, **n** takes one step and
pauses, **r** bulk-resets and creates a fresh paused simulation, **q** quits.
While running, generations advance approximately every 100 ms. Terminal resizing
only redraws; it never resizes the board or its arena. No editing, file loading,
speed controls, or additional game features.

## Memory ownership and lifecycle

The application makes **one game-owned malloc call at startup**. Capacity is
calculated from the actual sequence of simulation allocations using the same
checked alignment/layout function as the allocator. The arena borrows this
fundamentally aligned backing block; the application frees it at shutdown.

Each simulation makes exactly three arena allocations:

1. A `Life` struct containing dimensions, generation, and board pointers.
2. A byte-per-cell current board.
3. A byte-per-cell next board.

Both boards are explicitly initialized. Each generation writes the next board
and swaps existing pointers. There are **no per-generation allocations**.

```text
startup malloc → initialize arena → allocate/seed simulation
                                 → step repeatedly (no allocation)
                                 → discard simulation pointer
                                 → bulk reset
                                 → allocate/seed replacement simulation
                                 → ...
shutdown: discard simulation pointer → free startup backing block
```

Arena metadata lives on the application's stack, outside the reset region.
Reset preserves backing memory and lifetime statistics. All previous simulation
pointers become invalid logically, even if a new allocation reuses an address.
No retained old pointer may be used after reset.

## Allocator contract

- A nonzero request supplies a power-of-two alignment up to
  `_Alignof(max_align_t)`. Initialization requires nonempty backing storage
  aligned to `_Alignof(max_align_t)` and valid for its full stated capacity.
- Alignment padding is included in used bytes and peaks.
- Zero-size requests return `NULL` without validating alignment or altering
  the cursor/counters. The dry-run layout function likewise does nothing.
- Exhaustion, invalid nonzero-request alignment, invalid initialization, and
  arithmetic/counter overflow terminate with a diagnostic and `EXIT_FAILURE`.
  Validation happens before arena mutation. Checks do not depend on `assert`
  and remain enabled with `NDEBUG`.
- No runtime growth, fallback malloc, individual free, or pool allocator.
- `ARENA_DEBUG=1` fills the previously used region (including padding) with
  `0xDD` on reset. `ARENA_DEBUG=0` leaves bytes unchanged. This flag is independent
  of `NDEBUG`; new simulation state is explicitly initialized in either case.
- Reset clears current usage, simulation peak, and simulation allocation count.
  Lifetime peak, total successful nonzero allocations, and reset count survive.
  Each reset call counts, including one on an empty arena.
- Borrowed backing storage must not contain the arena metadata itself.

## Observe the allocator working

The application writes a flushed **arena-lifecycle.log** in its working
directory, replacing the previous run's report. Logging stays outside the
ncurses display and uses no game-owned runtime heap allocations.

1. Startup reports one backing allocation and the derived capacity.
2. Simulation creation reports three arena allocations and `used=capacity`.
3. Step or run for many generations, then restart or quit. The simulation-ended
   record has a larger generation number but identical usage/allocation counts.
4. Restart logs usage/local counters returning to zero, while lifetime counters
   survive. The replacement has three local allocations and six lifetime
   allocations after the first restart, still using the same backing block.
5. Shutdown reports one backing free and zero reserved bytes.

Tests make this evidence stronger: the lifecycle test runs 24 simulations and
4,800 generations with one backing block, checking every arena counter on every
step. Allocator tests cover alignment, padding, exact fit, zero-size requests,
reset/reuse, counters, and poisoning enabled/disabled. Fatal cases run in Linux
subprocesses, checking exit status and diagnostics in every configuration.
Simulation tests include a still life, an oscillator, a glider crossing wrapped
edges, a 1×1 board, explicit initialization after reset, and invalid dimensions.

**Limits:** counters alone cannot prove the absence of unrelated heap calls;
review heap call sites as well. stdio/ncurses internal allocations are outside
the game's rule. ASan/UBSan supplement tests, but a single backing block can hide
overruns between individual allocations, and a reset is not a system `free`.
Poisoning helps expose stale-data dependencies; it does not reliably detect
every stale pointer. No guard-allocation framework is added merely for testing.

## Source guide

- [arena.h](arena.h), [arena.c](arena.c): allocator contract and implementation.
- [life.h](life.h), [life.c](life.c): simulation layout, initialization, and rules.
- [main.c](main.c): startup ownership, minimal ncurses driver, and lifecycle log.
- [test_arena.c](test_arena.c), [test_life.c](test_life.c): allocation and simulation checks.
- [test_support.h](test_support.h): always-active checks and subprocess fatal tests.
- [Makefile](Makefile): separate debug, release, and sanitizer outputs.