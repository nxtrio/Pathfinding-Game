# Pathfinding Challenge — Player vs. Dijkstra vs. A*

An interactive C++17/SFML visualization for challenging two optimal pathfinding algorithms on the same grid. Draw your own route, watch Dijkstra and A* replay their searches step by step, and compare their exploration patterns, optimal path, and repeated-run benchmark results.

![Final Dijkstra and A* comparison overlay](assets/pathfinding-comparison.png)

The final overlay makes the central idea visible: Dijkstra spreads broadly according to cost from the start, while A* uses a goal-directed heuristic and usually expands less of this grid.

## Player vs. Algorithms

1. Draw a route from the emerald start cell to the coral target cell, or skip drawing and compare immediately.
2. Press Space or click **Compare**.
3. Dijkstra replays its frontier, expanded region, and reconstructed shortest path.
4. The board returns to its permanent colors before A* runs on the same obstacle layout.
5. A* replays with a separate amber/orange palette.
6. The final view combines both expanded regions, draws the optimal path above them, and reports search and benchmark metrics.

Player-path cells are traversable annotations, not walls. Only the permanent obstacle columns block either algorithm. When a player route is complete, the panel reports its length, steps above optimal, and efficiency:

```text
efficiency = optimal path length / player route length × 100
```

## Algorithms

### Dijkstra

Dijkstra prioritizes the smallest known cost from the start:

```text
priority(n) = g(n)
```

Every four-directional edge costs one, so its search expands uniformly outward until it reaches the target.

### A*

A* combines cost from the start with an estimate to the goal:

```text
f(n) = g(n) + h(n)
h(n) = Manhattan distance to the target
```

Manhattan distance is admissible and consistent for this unit-cost, four-directional grid. A* therefore retains optimality while usually directing substantially more work toward the target.

Both implementations use immutable priority-queue entries, deterministic tie-breaking, stale-entry skipping, and independent metadata resets. This avoids mutable heap-key bugs and makes repeated comparisons reproducible.

## Reading the Visualization

During each replay, a bright frontier color marks newly discovered cells and a deeper color marks expanded cells. The optimal route appears only after exploration finishes.

The final overlay uses:

- Cyan/blue: expanded only by Dijkstra
- Orange: expanded only by A*
- Violet: expanded by both algorithms
- Warm continuous line: shared optimal-path segments
- Thin cyan/orange lines: algorithm-specific segments if equally short paths differ

Start, target, obstacles, and player-path semantics remain intact throughout animation. Search traces store coordinates instead of pointers and never mutate permanent `NodeType` values.

## Comparison Metrics

| Metric | Definition |
| --- | --- |
| Path length | Number of edges in the reconstructed route (`path.size() - 1`) |
| Discovered | Unique cells that first receive a finite best-known cost and enter the logical frontier |
| Expanded | Unique cells whose current-best entry is removed and whose neighbors are processed |
| Peak frontier | Largest number of unique active frontier cells, excluding stale queue duplicates |
| Median search time | Median full-search duration from repeated, non-animated runs |

Expanded nodes are the primary educational comparison because they quantify useful search work and remain stable across machines. Runtime is secondary and can vary between runs.

## Benchmark Methodology

The benchmark is separate from animation time and runs once after both visual replays finish.

- 10 warm-up runs per algorithm
- 200 measured runs per algorithm
- Trace/event capture disabled
- Full search and path reconstruction included
- Pathfinding metadata reset by every solver invocation
- Same start, target, obstacles, and player-path state for every run
- Dijkstra/A* measurement order alternated between samples
- `std::chrono::steady_clock` durations summarized by the median

The panel labels the result as `MEDIAN SEARCH TIME (200 RUNS)`. Absolute timings depend on CPU, compiler, optimization level, operating-system scheduling, and other machine activity. Use a Release build when comparing them.

## Controls

| Input | Action |
| --- | --- |
| Left mouse drag | Draw player-route cells |
| Right mouse drag | Erase player-route cells |
| Space | Run or repeat Dijkstra → A* comparison |
| P | Pause or resume visualization |
| - / + | Decrease or increase visualization speed |
| R | Reset visualization and results; preserve player route |
| C | Clear player route, visualization, and results |
| Escape | Cancel visualization and return to editing |

The side panel also provides **Compare**, **Reset**, and **Clear Route** buttons. Start, target, and obstacle cells cannot be overwritten. Drag input is sampled continuously and interpolated between cells so quick strokes do not develop accidental gaps.

## Build and Run

Requirements:

- CMake 3.28 or newer
- A C++17 compiler
- SFML 3 Graphics

On macOS with Homebrew:

```bash
brew install sfml
```

Recommended Release build:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
./build-release/PathfindingGame
```

For a normal local development build:

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/PathfindingGame
```

CMake copies the `assets` directory beside the application after every build.

## Architecture

The implementation keeps one authoritative 40×30 logical grid and replays coordinate-based results sequentially. It does not duplicate the complete board for each algorithm.

```text
include/PathfindingGame.h       Grid, search results, metrics, solver APIs
src/PathfindingGame.cpp         Dijkstra, A*, reconstruction, benchmark
include/PathfindingAnimation.h  Game states, replay and overlay models
src/PathfindingAnimation.cpp    Non-blocking replay and final path rendering
include/ComparisonUI.h          Side-panel model and actions
src/ComparisonUI.cpp            Panel, metric bars, buttons, legend
src/Main.cpp                    SFML event loop, editing, state transitions
tests/PathfindingGameTests.cpp  Algorithm, animation, overlay, UI, benchmark tests
```

The frame loop polls events, advances clock-based animation steps, renders the board, and draws the persistent comparison panel. No sleeps, busy waits, UI framework, or separate rendering dependency are used.

## Testing

The CTest executable covers:

- Dijkstra/A* optimality and deterministic repeated runs
- Exact path reconstruction and obstacle avoidance
- Player-path traversability
- No-path behavior and stale-parent reset
- Discovered, expanded, and frontier metric sanity
- Animation timing, palette separation, restoration, and logical-state safety
- Final-overlay membership and shared/divergent path segments
- Panel actions, algorithm states, and player/A* percentage statements
- Repeated benchmark lifecycle and permanent-grid preservation

Run it directly through CTest:

```bash
ctest --test-dir build-release --output-on-failure
```

## Capturing a Demo

For a representative screenshot or GIF, capture the entire application window so the grid and comparison panel remain visible together. A useful sequence is:

1. Optionally draw a player route.
2. Select 2× or 4× speed.
3. Start at Dijkstra's frontier replay.
4. Continue through A*.
5. End on the final overlay with metrics and the 200-run median label visible.

Release builds are recommended for any captured benchmark values.
