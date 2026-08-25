#include "PathfindingGame.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <queue>

// Immutable priority snapshot used by both algorithms.
struct QueueEntry {
    double priority;
    double gSnapshot;
    double hSnapshot;
    std::size_t sequence;
    GridNode* node;
};

struct QueueEntryCompare {
    bool operator()(const QueueEntry& a, const QueueEntry& b) const {
        if (a.priority != b.priority) return a.priority > b.priority;
        if (a.hSnapshot != b.hSnapshot) return a.hSnapshot > b.hSnapshot;
        return a.sequence > b.sequence;
    }
};

void resetSolverState(std::vector<std::vector<GridNode*>>& grid) {
    for (auto& row : grid) {
        for (GridNode* node : row) {
            node->resetPathData();
        }
    }
}

// Helper to get valid neighbors (Up, Down, Left, Right)
std::vector<GridNode*> getNeighbors(GridNode* node,
                                    std::vector<std::vector<GridNode*>>& grid) {
    std::vector<GridNode*> neighbors;
    int x = node->x;
    int y = node->y;
    int rowCount = static_cast<int>(grid.size());
    int colCount = static_cast<int>(grid[y].size());

    if (x > 0)             neighbors.push_back(grid[y][x - 1]);     // Left
    if (x < colCount - 1)  neighbors.push_back(grid[y][x + 1]);     // Right
    if (y > 0)             neighbors.push_back(grid[y - 1][x]);     // Up
    if (y < rowCount - 1)  neighbors.push_back(grid[y + 1][x]);     // Down

    return neighbors;
}

double manhattanDistance(GridNode* node, GridNode* end) {
    return static_cast<double>(std::abs(node->x - end->x)
                             + std::abs(node->y - end->y));
}

std::vector<GridPosition> reconstructPath(GridNode* start, GridNode* end) {
    std::vector<GridPosition> path;

    for (GridNode* current = end; current != nullptr; current = current->parent) {
        path.push_back({current->x, current->y});
        if (current == start) {
            std::reverse(path.begin(), path.end());
            return path;
        }
    }

    return {};
}

SearchResult runSearch(std::vector<std::vector<GridNode*>>& grid,
                       GridNode* start,
                       GridNode* end,
                       bool captureSteps,
                       bool useHeuristic) {
    auto runStart = std::chrono::steady_clock::now();
    SearchResult result;
    resetSolverState(grid);

    std::vector<std::vector<bool>> discovered;
    std::vector<std::vector<bool>> expanded;
    std::vector<std::vector<bool>> inFrontier;
    for (const auto& row : grid) {
        discovered.push_back(std::vector<bool>(row.size(), false));
        expanded.push_back(std::vector<bool>(row.size(), false));
        inFrontier.push_back(std::vector<bool>(row.size(), false));
    }

    std::priority_queue<QueueEntry,
                        std::vector<QueueEntry>,
                        QueueEntryCompare> frontier;
    std::size_t sequence = 0;
    std::size_t frontierSize = 1;

    start->gCost = 0.0;
    start->hCost = useHeuristic ? manhattanDistance(start, end) : 0.0;
    start->fCost = start->gCost + start->hCost;
    start->parent = nullptr;

    discovered[start->y][start->x] = true;
    inFrontier[start->y][start->x] = true;
    result.metrics.discoveredNodes = 1;
    result.metrics.maxFrontierSize = 1;
    frontier.push({start->fCost, start->gCost, start->hCost,
                   sequence++, start});

    while (!frontier.empty()) {
        QueueEntry entry = frontier.top();
        frontier.pop();
        GridNode* current = entry.node;

        if (entry.gSnapshot != current->gCost) continue;
        if (expanded[current->y][current->x]) continue;

        if (inFrontier[current->y][current->x]) {
            inFrontier[current->y][current->x] = false;
            --frontierSize;
        }

        if (current == end) {
            result.path = reconstructPath(start, end);
            result.metrics.found = !result.path.empty();
            if (result.metrics.found) {
                result.metrics.pathLength = static_cast<int>(result.path.size()) - 1;
            }
            break;
        }

        expanded[current->y][current->x] = true;
        ++result.metrics.expandedNodes;
        if (captureSteps && current != start) {
            result.steps.push_back({{current->x, current->y}, EXPANDED});
        }

        for (GridNode* neighbor : getNeighbors(current, grid)) {
            if (neighbor->type == OBSTACLE) continue;
            if (expanded[neighbor->y][neighbor->x]) continue;

            double tempG = current->gCost + 1.0;
            if (tempG >= neighbor->gCost) continue;

            bool firstDiscovery = !discovered[neighbor->y][neighbor->x];
            neighbor->gCost = tempG;
            neighbor->hCost = useHeuristic ? manhattanDistance(neighbor, end) : 0.0;
            neighbor->fCost = neighbor->gCost + neighbor->hCost;
            neighbor->parent = current;

            if (firstDiscovery) {
                discovered[neighbor->y][neighbor->x] = true;
                inFrontier[neighbor->y][neighbor->x] = true;
                ++result.metrics.discoveredNodes;
                ++frontierSize;
                result.metrics.maxFrontierSize = std::max(
                    result.metrics.maxFrontierSize, frontierSize
                );
            }

            if (captureSteps && neighbor != start && neighbor != end) {
                result.steps.push_back({{neighbor->x, neighbor->y}, DISCOVERED});
            }

            frontier.push({neighbor->fCost, neighbor->gCost, neighbor->hCost,
                           sequence++, neighbor});
        }
    }

    auto runEnd = std::chrono::steady_clock::now();
    result.metrics.singleRunMicroseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(runEnd - runStart).count();
    return result;
}

// --- Dijkstra Implementation ---
SearchResult Dijkstra::solve(std::vector<std::vector<GridNode*>>& grid,
                             GridNode* start,
                             GridNode* end,
                             bool captureSteps) {
    return runSearch(grid, start, end, captureSteps, false);
}

// --- A* Implementation ---
SearchResult AStar::solve(std::vector<std::vector<GridNode*>>& grid,
                          GridNode* start,
                          GridNode* end,
                          bool captureSteps) {
    return runSearch(grid, start, end, captureSteps, true);
}

AlgorithmComparison beginAlgorithmComparison(
    std::vector<std::vector<GridNode*>>& grid,
    GridNode* start,
    GridNode* end,
    bool captureSteps)
{
    Dijkstra dijkstra;
    AlgorithmComparison comparison;

    comparison.dijkstra = dijkstra.solve(grid, start, end, captureSteps);
    return comparison;
}

void completeAlgorithmComparison(
    AlgorithmComparison& comparison,
    std::vector<std::vector<GridNode*>>& grid,
    GridNode* start,
    GridNode* end,
    bool captureSteps)
{
    AStar astar;
    comparison.astar = astar.solve(grid, start, end, captureSteps);

    bool bothFound = comparison.dijkstra.metrics.found &&
                     comparison.astar.metrics.found;
    bool neitherFound = !comparison.dijkstra.metrics.found &&
                        !comparison.astar.metrics.found;
    bool samePathLength = comparison.dijkstra.metrics.pathLength ==
                          comparison.astar.metrics.pathLength;

    if (bothFound && samePathLength) {
        comparison.status = MATCHING_PATHS;
    } else if (neitherFound) {
        comparison.status = BOTH_NO_PATH;
    } else {
        comparison.status = MISMATCHED_RESULTS;
    }

    comparison.available = true;
}

AlgorithmComparison runAlgorithmComparison(
    std::vector<std::vector<GridNode*>>& grid,
    GridNode* start,
    GridNode* end,
    bool captureSteps)
{
    AlgorithmComparison comparison = beginAlgorithmComparison(
        grid, start, end, captureSteps
    );
    completeAlgorithmComparison(
        comparison, grid, start, end, captureSteps
    );

    return comparison;
}
