#ifndef PATHFINDINGGAME_H
#define PATHFINDINGGAME_H

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <limits>
#include <vector>

// Grid cells keep a stable size in world coordinates. Active dimensions come
// from the grid container rather than global row/column constants.
inline constexpr int CELL_SIZE = 25;

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

enum ComparisonStatus { MATCHING_PATHS, BOTH_NO_PATH, MISMATCHED_RESULTS };

struct BenchmarkMetrics {
    bool available = false;
    std::size_t warmupRuns = 0;
    std::size_t measuredRuns = 0;
    long long dijkstraMedianNanoseconds = 0;
    long long astarMedianNanoseconds = 0;
};

struct AlgorithmComparison {
    SearchResult dijkstra;
    SearchResult astar;
    BenchmarkMetrics benchmark;
    ComparisonStatus status = MISMATCHED_RESULTS;
    bool available = false;
};

class GridNode;
using Grid = std::vector<std::vector<GridNode*>>;

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
        shape.setFillColor(sf::Color(24, 31, 47));
    }

    void setType(NodeType t) {
        type = t;
        switch (type) {
            case EMPTY:
                shape.setFillColor(sf::Color(24, 31, 47));
                break;
            case PLAYER_PATH:
                shape.setFillColor(sf::Color(91, 65, 145));
                break;
            case START:
                shape.setFillColor(sf::Color(36, 200, 120));
                break;
            case END:
                shape.setFillColor(sf::Color(238, 82, 104));
                break;
            case OBSTACLE:
                shape.setFillColor(sf::Color(43, 38, 58));
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

// Grid lifetime and interaction helpers used when maps are rebuilt.
Grid createGrid(int rows, int cols);
void destroyGrid(Grid& grid);
// On success, all pointers into the previous grid are invalidated.
void recreateGrid(Grid& grid, int rows, int cols);

int computeHandDrawnPathLength(const Grid& grid,
                               GridNode* startNode,
                               GridNode* endNode);

void editGridLine(Grid& grid,
                  GridNode* startNode,
                  GridNode* endNode,
                  int startX,
                  int startY,
                  int endX,
                  int endY,
                  NodeType editType);

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

AlgorithmComparison runAlgorithmComparison(
    std::vector<std::vector<GridNode*>>& grid,
    GridNode* start,
    GridNode* end,
    bool captureSteps = true
);

AlgorithmComparison beginAlgorithmComparison(
    std::vector<std::vector<GridNode*>>& grid,
    GridNode* start,
    GridNode* end,
    bool captureSteps = true
);

void completeAlgorithmComparison(
    AlgorithmComparison& comparison,
    std::vector<std::vector<GridNode*>>& grid,
    GridNode* start,
    GridNode* end,
    bool captureSteps = true
);

BenchmarkMetrics benchmarkAlgorithms(
    std::vector<std::vector<GridNode*>>& grid,
    GridNode* start,
    GridNode* end,
    std::size_t warmupRuns = 10,
    std::size_t measuredRuns = 200
);

#endif
