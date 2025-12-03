# Pathfinding Game (SFML)

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
  - **Wall** (dark grey) – user-drawn path segments.
  - **Empty** (white) – open space.

- **Hand-drawn path detection**

  - As you draw walls, the game runs a BFS over the grid to see if your **walls + start + end** form a continuous path.
  - Once a path exists, the game displays the path length in the HUD and then exits.

- **Pathfinding algorithms (in code)**

  - `Dijkstra` and `AStar` classes are implemented in `PathfindingGame.cpp` and operate over the same `GridNode` grid.
  - They are not currently hooked up to the main game loop or renderer, but are available for future visualization / comparison features.

---

## Controls

- **Left mouse button** – draw a wall on the hovered cell  
  (not allowed on Start, End, or Obstacle cells).

- **Right mouse button** – erase a wall  
  (not allowed on Start, End, or Obstacle cells).

- When your walls create a continuous path from **start** to **end**, the game:

  - Computes the path length (number of steps),
  - Updates the HUD text,
  - Then exits.

---

## How It Works (High-Level Code Overview)

### 1. `GridNode` and `NodeType` (`PathfindingGame.h`)

Each cell in the grid is represented by:

```cpp
class GridNode {
public:
    int x, y;
    NodeType type;           // EMPTY, WALL, START, END, VISITED, PATH, FRONTIER, OBSTACLE
    double gCost, hCost, fCost;
    GridNode* parent;
    sf::RectangleShape shape;
    // ...
};
