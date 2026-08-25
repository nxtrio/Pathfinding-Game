#ifndef PATHFINDINGGAME_H
#define PATHFINDINGGAME_H

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <limits>
#include <vector>

// Constants for Grid
const int ROW_COUNT = 30;
const int COL_COUNT = 40;
const int CELL_SIZE = 25;

// Enum for permanent/logical node state
enum NodeType { EMPTY, PLAYER_PATH, START, END, OBSTACLE };

enum SearchStepType { DISCOVERED, EXPANDED };

struct GridPosition {
    int x;
    int y;

    bool operator==(const GridPosition& other) const {
        return x == other.x && y == other.y;
    }
};

struct SearchStep {
    GridPosition position;
    SearchStepType type;
};

struct SearchMetrics {
    bool found = false;
    int pathLength = -1;
    std::size_t discoveredNodes = 0;
    std::size_t expandedNodes = 0;
    std::size_t maxFrontierSize = 0;
    long long singleRunMicroseconds = 0;
};

struct SearchResult {
    std::vector<SearchStep> steps;
    std::vector<GridPosition> path;
    SearchMetrics metrics;
};

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
            case PLAYER_PATH:
                shape.setFillColor(sf::Color(50, 50, 50)); // dark grey player route
                break;
            case START:
                shape.setFillColor(sf::Color::Green);
                break;
            case END:
                shape.setFillColor(sf::Color::Red);
                break;
            case OBSTACLE:
                // permanent obstacle – purple-ish so it stands out
                shape.setFillColor(sf::Color(150, 0, 150));
                break;
        }
    }

    // Reset costs for a new run
    void resetPathData() {
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

    virtual SearchResult solve(std::vector<std::vector<GridNode*>>& grid,
                               GridNode* start,
                               GridNode* end,
                               bool captureSteps = true) = 0;
};

// --- 3. Concrete Implementation: Dijkstra ---
class Dijkstra : public Pathfinder {
public:
    SearchResult solve(std::vector<std::vector<GridNode*>>& grid,
                       GridNode* start,
                       GridNode* end,
                       bool captureSteps = true) override;
};

// --- 4. Concrete Implementation: A* ---
class AStar : public Pathfinder {
public:
    SearchResult solve(std::vector<std::vector<GridNode*>>& grid,
                       GridNode* start,
                       GridNode* end,
                       bool captureSteps = true) override;
};

#endif
