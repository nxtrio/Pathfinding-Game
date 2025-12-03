#ifndef PATHFINDINGGAME_H
#define PATHFINDINGGAME_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <queue>
#include <cmath>
#include <iostream>
#include <stack>
#include <unordered_map>
#include <limits>

// Constants for Grid
const int ROW_COUNT = 30;
const int COL_COUNT = 40;
const int CELL_SIZE = 25;

// Enum for Node State (Visual Representation)
enum NodeType { EMPTY, WALL, START, END, VISITED, PATH, FRONTIER, OBSTACLE };

// --- 1. The Node Class ---
// Represents a single square on the grid.
class GridNode {
public:
    int x, y;           // Grid coordinates
    NodeType type;      // State of the node

    // Pathfinding Costs
    double gCost;       // Distance from start
    double hCost;       // Heuristic distance to end (A*)
    double fCost;       // Total cost (G + H)
    GridNode* parent;   // For backtracking the path

    sf::RectangleShape shape; // SFML Visual component

    GridNode(int col, int row)
        : x(col),
          y(row),
          type(EMPTY),
          gCost(std::numeric_limits<double>::infinity()),
          hCost(0.0),
          fCost(std::numeric_limits<double>::infinity()),
          parent(nullptr),
          shape(sf::Vector2f(static_cast<float>(CELL_SIZE - 1),
                             static_cast<float>(CELL_SIZE - 1))) // -1 for grid line effect
    {
        shape.setPosition(sf::Vector2f(
            static_cast<float>(x * CELL_SIZE),
            static_cast<float>(y * CELL_SIZE)
        ));
        shape.setFillColor(sf::Color::White);
    }

    void setType(NodeType t) {
        type = t;
        switch (type) {
            case EMPTY:
                shape.setFillColor(sf::Color::White);
                break;
            case WALL:
                shape.setFillColor(sf::Color(50, 50, 50)); // dark grey
                break;
            case START:
                shape.setFillColor(sf::Color::Green);
                break;
            case END:
                shape.setFillColor(sf::Color::Red);
                break;
            case VISITED:
                shape.setFillColor(sf::Color(100, 200, 255)); // light blue
                break;
            case PATH:
                shape.setFillColor(sf::Color::Yellow);
                break;
            case FRONTIER:
                shape.setFillColor(sf::Color(100, 255, 100)); // light green
                break;
            case OBSTACLE:
                // permanent obstacle – purple-ish so it stands out
                shape.setFillColor(sf::Color(150, 0, 150));
                break;
        }
    }

    // Reset costs for a new run
    void resetPathData() {
        // Don’t clear WALL, START, END, or OBSTACLE
        if (type != WALL && type != START && type != END && type != OBSTACLE) {
            setType(EMPTY);
        }
        gCost = std::numeric_limits<double>::infinity();
        hCost = 0.0;
        fCost = std::numeric_limits<double>::infinity();
        parent = nullptr;
    }
};

// --- 2. Modular Algorithm Engine ---
class Pathfinder {
public:
    virtual ~Pathfinder() = default; // we delete via base pointer

    virtual void solve(std::vector<std::vector<GridNode*>>& grid,
                       GridNode* start,
                       GridNode* end,
                       std::vector<GridNode*>& nodesToAnimate) = 0;
};

// --- 3. Concrete Implementation: Dijkstra ---
class Dijkstra : public Pathfinder {
public:
    void solve(std::vector<std::vector<GridNode*>>& grid,
               GridNode* start,
               GridNode* end,
               std::vector<GridNode*>& nodesToAnimate) override;
};

// --- 4. Concrete Implementation: A* ---
class AStar : public Pathfinder {
public:
    void solve(std::vector<std::vector<GridNode*>>& grid,
               GridNode* start,
               GridNode* end,
               std::vector<GridNode*>& nodesToAnimate) override;
};

#endif
