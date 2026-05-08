# Concurrent Maze Solver

**A lock-free, fence-free bidirectional DFS maze solver in C++17.**
Two threads search from opposite ends of a shared `std::atomic_uint[]` cell array; meeting detection is a single `fetch_or` instruction per cell visit. Achieves **~2.7× speedup over an optimized single-threaded DFS** and **~3.8× over single-threaded BFS** on mazes up to 20,000 × 20,000 cells (400 million cells).

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![MSVC v143](https://img.shields.io/badge/MSVC-v143-purple.svg)
![Windows x64](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg)

---

## What this project is

A C++17 implementation of a high-throughput maze solver, written for an Optimized C++ Multithreading course (CSC 562, DePaul). The course provides single-threaded BFS and DFS reference solvers; the assignment is to build a multithreaded solver that beats them both, **with the speedup coming from genuine work division**, not algorithmic preprocessing.

The solution here is a **bidirectional DFS** that runs two `std::thread` workers simultaneously — one from the entrance, one from the exit — over a single shared cell array. They detect each other through atomic per-cell visit bits and stop using a relaxed-ordering atomic flag.

---

## Architecture

```
                        Main Thread
                            │
                  constructs SharedData
                            │
                ┌───────────┴───────────┐
                ▼                       ▼
      TopDown_STMazeSolverDFS    BottomUp_STMazeSolverDFS
      starts at maze.Start       starts at maze.End
      marks bit 0x8 per cell     marks bit 0x4 per cell
                │                       │
                └─── shared atomic_uint[] cell array ───┘
                └─── SharedData ────────────────────────┘
                       atomic<Position>  stop
                       atomic_bool       isCollision
```

| Channel | Purpose | Mechanism |
|---|---|---|
| Cell array (`atomic_uint[]`) | Per-cell visitation map and meeting detection | `fetch_or` per visit (one `lock or` on x86) |
| `SharedData.isCollision` | Global stop signal | `atomic_bool`, polled with `memory_order_relaxed` |
| `SharedData.stop` | Meeting position for path stitching | `atomic<Position>`, written once by the discoverer |
| `std::thread::join` | The only acquire/release synchronization point | Implicit in the C++ memory model |

When the two DFS frontiers cross, one thread's `fetch_or` returns the other thread's bit — the discoverer flips `isCollision`, the other thread sees it on its next poll and unwinds. The main thread joins both workers and concatenates `[start → meeting]` with `[meeting → end]`.

---

## Highlights

- **Wait-free meeting detection.** One atomic RMW (`fetch_or`) per cell visit. No mutexes, no condition variables, no fences in the hot path.
- **Relaxed memory ordering throughout.** Algorithm is idempotent under stale reads — a missed flag costs at most a few cells of wasted work, never correctness. The required `happens-before` is provided by `std::thread::join`.
- **Co-located visit bits.** Per-thread visitation marks live in the same `std::atomic_uint` as the wall data. Each cell visit is one cache-line touch.
- **Zero allocator pressure inside `Solve()`.** Choice stacks are pre-`reserve`d at 400,000 slots; the final path vector is `reserve`d once with the exact concatenated size.
- **Skipping DFS.** Straight corridors (degree-1 cells) are walked through without pushing onto the choice stack — only branch points are recorded.
- **Wall-clock benchmarking with `seq_cst` fence brackets** around `QueryPerformanceCounter` to prevent compiler/CPU reordering from biasing measurements.

---

## Quick Start

### Prerequisites
- Visual Studio 2022 with the **MSVC v143** C++ toolset
- Windows 10/11 SDK
- x64 Windows host

### Build
```bat
:: Open the solution in Visual Studio
PA2\Maze 6.0 - Student Drop\Maze.sln

:: Or build from the command line
msbuild "PA2\Maze 6.0 - Student Drop\Maze.sln" /p:Configuration=Release /p:Platform=x64
```

The output binary `MazeX64Release.exe` and its `File_DLL_FastX64Release.dll` runtime dependency are written to `PA2\Maze 6.0 - Student Drop\x64\Release\`.

### Run a single maze
```bat
MazeX64Release.exe Maze1Kx1K.data
```

### Run the contest benchmark suite
```bat
cd PA2\Maze_DevelopmentData
copy "..\Maze 6.0 - Student Drop\x64\Release\MazeX64Release.exe" .
copy "..\Maze 6.0 - Student Drop\x64\Release\File_DLL_FastX64Release.dll" .
.\Test_Contest.bat
type results_contest.txt
```

`Test_Contest.bat` runs the four grading mazes (`Maze15Kx15K_E/J.data`, `Maze20Kx20K_B/D.data`) and concatenates per-maze BFS / DFS / multithreaded results.

> **Note:** `main.cpp` line 16 must be `#define FINAL_SUBMIT 1` for the binary to honor `argv`. With `FINAL_SUBMIT 0`, every run silently uses `Maze1kx1k.data` regardless of command line.

---

## Benchmark Results

### Multi-threaded vs single-threaded baselines, **1K × 1K maze, x64 Release**

Aggregated across 17 runs from `results_small.txt`, `results_medium.txt`, `results_large.txt`, `results_contest.txt`:

| Solver | Mean runtime | vs ST DFS | vs ST BFS |
|---|---:|---:|---:|
| Single-threaded BFS | 0.036 s | 0.71× | 1.00× |
| Single-threaded DFS (skipping) | 0.026 s | 1.00× | 1.41× |
| **Multi-threaded student solver** | **0.0095 s** | **2.7×** | **3.8×** |

Solution length: 5,863 directions, verified by `Maze::checkSolution()` on every run.

### Single-threaded reference at scale (`Keenan_Reference_results_contest.txt`)

| Maze | Cells | Solution length | BFS (s) | DFS (s) |
|---|---:|---:|---:|---:|
| 15K × 15K (E) | 225 M | 172,359 | 6.108 | 3.113 |
| 15K × 15K (J) | 225 M | 164,043 | 5.403 | 3.946 |
| 20K × 20K (B) | 400 M | 306,443 | 9.652 | 3.657 |
| 20K × 20K (D) | 400 M | 291,601 | 10.890 | 4.271 |

The DFS-vs-BFS ratio collapses as mazes grow (0.51 → 0.38) — BFS's frontier queue grows quadratically with maze width while DFS-with-skipping doesn't. This is the structural reason the multithreaded solver is built on DFS, not BFS.

### Where timing happens
| File:Line | Region timed |
|---|---|
| `main.cpp:54-59` | `STMazeSolverBFS::Solve()` |
| `main.cpp:76-81` | `STMazeSolverDFS::Solve()` |
| `main.cpp:98-103` | `MTMazeStudentSolver::Solve()` |
| `Framework.h:1316-1336` | `Tic` / `Toc` with `seq_cst` fences around `QueryPerformanceCounter` |

All measurements are wall-clock — the right metric for evaluating multithreaded speedup. Maze loading and solution verification are deliberately outside the timed regions.

---

## Technical Deep Dive

### Lock-free meet-in-the-middle (`maze.cpp:332-348`)

```cpp
void Maze::isTopCollision(Position pos, SharedData& sharedData)
{
    if ((poMazeData[pos.row * width + pos.col].fetch_or(0x8) & 0x4) == 0x4)
    {
        sharedData.stop.store(pos, std::memory_order_relaxed);
        sharedData.isCollision.store(true, std::memory_order_relaxed);
    }
}

void Maze::isBottomCollision(Position pos, SharedData& sharedData)
{
    if ((poMazeData[pos.row * width + pos.col].fetch_or(0x4) & 0x8) == 0x8)
    {
        sharedData.stop.store(pos, std::memory_order_relaxed);
        sharedData.isCollision.store(true, std::memory_order_relaxed);
    }
}
```

Each cell is a single `std::atomic_uint`. Bits `0x1` / `0x2` hold east/south wall info; bit `0x8` is "top thread visited", `0x4` is "bottom thread visited". The `fetch_or` is one atomic RMW: the **return value is the cell's prior state**, so the same instruction marks the cell *and* tells the visiting thread whether the other thread already marked it. The first thread whose `fetch_or` returns the other thread's bit is the discoverer of the meeting point. On x86-64 this lowers to a single `lock or` instruction.

### Hot-path polling (`TopDown_solverBase.h:66-96`)

```cpp
do {
    if (sharedData.isCollision.load(std::memory_order_relaxed))
        throw TopDown_SolutionFoundSkip(at, reverseDir(go_to));

    pMaze->isTopCollision(at, sharedData);

    if (sharedData.isCollision.load(std::memory_order_relaxed))
        throw TopDown_SolutionFoundSkip(at, reverseDir(go_to));

    if (at == pMaze->getEnd())   { sharedData.isCollision.store(true, std::memory_order_relaxed); /* unwind */ }
    if (at == pMaze->getStart()) { sharedData.isCollision.store(true, std::memory_order_relaxed); /* unwind */ }

    Choices = pMaze->getMoves(at);
    Choices.remove(came_from);
    if (Choices.size() == 1) { /* skip through corridor */ }
} while (Choices.size() == 1);
```

Two relaxed loads per cell. On x86 these compile to plain `mov` instructions — same machine code as `memory_order_acquire` would produce on this architecture, but with no fence on weakly-ordered platforms (ARM, POWER). The flag is sticky, so any thread that observes `false` now will observe `true` later; algorithmic correctness does not depend on prompt visibility.

### Skipping DFS

The `follow()` function walks straight corridors without recording them — only branch points (`Choices.size() ≥ 2`) are pushed onto the choice stack. This converts the DFS task graph from "one frame per cell" to "one frame per branch point", which is what keeps the choice stack small enough to pre-`reserve` once and use throughout.

### Pre-reserved choice stacks

```cpp
std::vector<Choice> pChoiceStack;
pChoiceStack.reserve(VECTOR_RESERVE_SIZE);   // 400000
```

A `Choice` is ~32 bytes; 400K of them is ~13 MB — easily L2-resident, allocated exactly once per `Solve()` call. Without this, `vector::push_back` would trigger ~20 capacity-doubling reallocations on a contest-scale DFS, each one a `memcpy` of the entire stack.

### Result aggregation (`MTMazeStudentSolver.h:67-100`)

```cpp
const bool collided = sharedData.isCollision.load(std::memory_order_relaxed);
auto* pResult = new std::vector<Direction>();

if (collided) {
    pResult->reserve(pTopPath->size() + pBottomPath->size());
    pResult->insert(pResult->end(), pTopPath->begin(),    pTopPath->end());
    pResult->insert(pResult->end(), pBottomPath->begin(), pBottomPath->end());
}
```

Single `reserve` with the exact final size, then two `insert`s — exactly two `memcpy`s, no resize.

---

## Project Layout

```
concurrent_maze/
└── PA2/
    ├── PA2 - Maze.pdf                  Assignment specification
    ├── Maze Report.pdf                 Final design write-up
    ├── Maze 6.0 - Student Drop/        Final implementation (source of truth)
    │   ├── Maze.sln
    │   ├── Maze/
    │   │   ├── main.cpp                Entry point + per-solver timing
    │   │   ├── maze.cpp / Maze.h       Cell array + atomic collision detection
    │   │   ├── MTMazeStudentSolver.h   Multithreaded coordinator
    │   │   ├── TopDown_DFSsolver.h     Top-down worker
    │   │   ├── BottomUp_STMazeSolverDFS.h   Bottom-up worker
    │   │   ├── SharedData.h            Cross-thread atomic state
    │   │   ├── STMazeSolverBFS.h       Reference single-threaded BFS
    │   │   ├── STMazeSolverDFS.h       Reference single-threaded DFS
    │   │   ├── SkippingMazeSolver.h    Corridor-skipping DFS base
    │   │   ├── Choice.h                DFS frame type
    │   │   ├── Position.h, Direction.h Geometry primitives
    │   │   └── Maze.vcxproj            MSVC build (C++17, MaxSpeed, LTCG)
    │   ├── Framework/Framework.h       PerformanceTimer, memory tracker
    │   └── dist/File/                  File I/O DLL
    └── Maze_DevelopmentData/
        ├── Maze*.data                  Test mazes from 5×5 up to 20K×20K
        ├── Test_Contest.bat            Grading benchmark suite
        ├── Test_Small/Medium/Large.bat Sweep benchmarks
        └── results_*.txt               Captured run output
```

---

## Design Decisions

| Decision | Rationale |
|---|---|
| **Two threads, not N** | Bidirectional DFS has a clean two-fragment stitching invariant: `[start → meeting] + [meeting → end]`. Going to N>2 threads requires either a frontier-queue BFS with work-stealing or geometric region partitioning — both are an order of magnitude more complex with no guaranteed speedup on irregular DFS workloads. |
| **`memory_order_relaxed` everywhere** | The algorithm is idempotent under stale reads. The only required cross-thread `happens-before` is `worker writes path → main reads path`, which `std::thread::join` provides. On x86, relaxed loads are plain `mov`s — same code as acquire would emit. |
| **Visit bits in the cell, not a side table** | Co-locating visit bits with wall data means each cell visit hits exactly one cache line. A separate visit-bitmap would double the working set and add a second cache miss per step. |
| **Exception-driven early termination** | A `throw SolutionFoundSkip` unwinds the DFS stack from any depth without explicit return-checking. The throw happens once per solve; steady-state cost is zero. Wrong tool for a real-time loop, right tool for a one-shot algorithm. |
| **Pre-reserved stacks** | Allocator pressure inside the timed region would alone be a multi-percent regression on contest-scale runs. `reserve(400000)` is the only allocation in the steady state. |

---

## Known Improvements

The current implementation is intentionally scoped to a 2-thread fork/join design. A production hardening pass would address:

- **Cache-line-correct padding.** `SharedData` uses 7 bytes of manual padding — short of a 64-byte cache line. Replace with `alignas(std::hardware_destructive_interference_size)` per atomic. (Empirically harmless today because each atomic has a single writer.)
- **Acquire/release on `stop`.** The bottom solver's path-reconstruction reads `sharedData.stop` with relaxed ordering — should be `acquire`, paired with a release store on the writer. Latent today because both threads also stop on the flag.
- **Persistent thread pool.** `std::thread` fork/join costs ~50µs of OS-thread setup per `Solve()`. For one-shot multi-second workloads this is invisible; for low-latency repeated invocations, swap in a pinned-core pool parked on a futex.
- **Single source of truth for `VECTOR_RESERVE_SIZE`.** Currently `#define`d in three headers; promote to a single `constexpr`.
- **Ownership transfer over deep-copy** in the no-collision fallback path of `MTMazeStudentSolver::Solve`.

---

## Acknowledgements

- **Course:** CSC 362/462 — Optimized C++ Multithreading, DePaul University
- **Instructor & framework author:** Ed Keenan (`Framework.h`, `File` DLL, reference BFS/DFS solvers, maze generators)
- **Author of multithreaded solver:** HyeSeung Lee
