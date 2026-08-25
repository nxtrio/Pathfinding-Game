# Pathfinding Game

A simple grid-based pathfinding “puzzle” game built with **C++17** and **SFML**.

You draw a path from a green **start** square to a red **end** square while avoiding purple **obstacle** columns.  
When your hand-drawn path successfully connects start to end, the game detects it and exits.

> Note: The project also contains implementations of **Dijkstra** and **A\*** pathfinding algorithms in the codebase (`PathfindingGame.cpp`), but the current version of the game UI focuses on **hand-drawn path detection** and does not yet visualize the algorithmic paths.

---

## Features

- **Grid-based board**

  - 40 columns × 30 rows of cells.
  - Each cell is a `GridNode` with a type and a `sf::RectangleShape` for rendering.

- **Special cell types**

  - **Start** (green square)
  - **End** (red square)
  - **Obstacle** (purple columns) – pre-generated and **cannot** be edited by the user.
  - **Player path** (dark grey) – user-drawn, traversable route segments.
  - **Empty** (white) – open space.

- **Hand-drawn path detection**

  - As you draw a route, the game runs a BFS over the grid to see if your **player path + start + end** form a continuous path.
  - Once a path exists, the game displays the path length in the HUD and then exits.

- **Pathfinding algorithms (in code)**

  - `Dijkstra` and `AStar` classes are implemented in `PathfindingGame.cpp` and operate over the same `GridNode` grid.
  - Each run returns a deterministic `SearchResult` containing trace events, the reconstructed optimal path, and search metrics.
  - Player-path cells remain traversable; only permanent obstacles block the algorithms.
  - They are not currently hooked up to the main game loop or renderer, but are available for future visualization / comparison features.

---

## Controls

- **Left mouse button** – draw the player path on the hovered cell
  (not allowed on Start, End, or Obstacle cells).

- **Right mouse button** – erase the player path
  (not allowed on Start, End, or Obstacle cells).

- When your route creates a continuous path from **start** to **end**, the game:

  - Computes the path length (number of steps),
  - Updates the HUD text,
  - Then exits.

---

## Build and Run

The project requires a C++17 compiler, CMake, and SFML 3. On macOS, SFML can be installed with Homebrew:

```bash
brew install sfml
```

Configure, build, and run the game from the repository root:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/PathfindingGame
```

---

## How It Works (High-Level Code Overview)

### 1. `GridNode` and `NodeType` (`PathfindingGame.h`)

Each cell in the grid is represented by:

```cpp
class GridNode {
public:
    int x, y;
    NodeType type;           // EMPTY, PLAYER_PATH, START, END, OBSTACLE
    double gCost, hCost, fCost;
    GridNode* parent;
    sf::RectangleShape shape;
    // ...
};
