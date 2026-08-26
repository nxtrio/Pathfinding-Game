#include "PathfindingGame.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <queue>
#include <stdexcept>
#include <utility>

Grid createGrid(int rows, int cols) {
    if (rows <= 0 || cols <= 0) {
        throw std::invalid_argument("grid dimensions must be positive");
    }

    Grid grid(
        static_cast<std::size_t>(rows),
        std::vector<GridNode*>(static_cast<std::size_t>(cols), nullptr)
    );

    try {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                grid[y][x] = new GridNode(x, y);
            }
        }
    } catch (...) {
        destroyGrid(grid);
        throw;
    }

    return grid;
}

void destroyGrid(Grid& grid) {
    for (auto& row : grid) {
        for (GridNode*& node : row) {
            delete node;
            node = nullptr;
        }
    }
    grid.clear();
}

void recreateGrid(Grid& grid, int rows, int cols) {
    Grid replacement = createGrid(rows, cols);
    destroyGrid(grid);
    grid = std::move(replacement);
}

int computeHandDrawnPathLength(const Grid& grid,
                               GridNode* startNode,
                               GridNode* endNode) {
    if (grid.empty() || startNode == nullptr || endNode == nullptr) return -1;

    auto containsNode = [&](GridNode* node) {
        return node->y >= 0 &&
               node->y < static_cast<int>(grid.size()) &&
               node->x >= 0 &&
               node->x < static_cast<int>(grid[node->y].size()) &&
               grid[node->y][node->x] == node;
    };
    if (!containsNode(startNode) || !containsNode(endNode)) return -1;

    std::vector<std::vector<int>> distance;
    distance.reserve(grid.size());
    for (const auto& row : grid) {
        distance.emplace_back(row.size(), -1);
    }

    std::queue<GridPosition> frontier;
    distance[startNode->y][startNode->x] = 0;
    frontier.push({startNode->x, startNode->y});

    auto canWalk = [](GridNode* node) {
        return node->type == START || node->type == PLAYER_PATH ||
               node->type == END;
    };

    constexpr int DX[4] = {1, -1, 0, 0};
    constexpr int DY[4] = {0, 0, 1, -1};

    while (!frontier.empty()) {
        GridPosition current = frontier.front();
        frontier.pop();

        if (current.x == endNode->x && current.y == endNode->y) {
            return distance[current.y][current.x];
        }

        for (int direction = 0; direction < 4; ++direction) {
            int nextX = current.x + DX[direction];
            int nextY = current.y + DY[direction];
            if (nextY < 0 || nextY >= static_cast<int>(grid.size()) ||
                nextX < 0 ||
                nextX >= static_cast<int>(grid[nextY].size()) ||
                distance[nextY][nextX] != -1) {
                continue;
            }

            GridNode* neighbor = grid[nextY][nextX];
            if (!canWalk(neighbor)) continue;

            distance[nextY][nextX] = distance[current.y][current.x] + 1;
            frontier.push({nextX, nextY});
        }
    }

    return -1;
}

void editGridLine(Grid& grid,
                  GridNode* startNode,
                  GridNode* endNode,
                  int startX,
                  int startY,
                  int endX,
                  int endY,
                  NodeType editType) {
    int x = startX;
    int y = startY;
    int dx = std::abs(endX - startX);
    int dy = std::abs(endY - startY);
    int stepX = startX < endX ? 1 : -1;
    int stepY = startY < endY ? 1 : -1;
    int error = dx - dy;

    while (true) {
        if (y >= 0 && y < static_cast<int>(grid.size()) && x >= 0 &&
            x < static_cast<int>(grid[y].size())) {
            GridNode* node = grid[y][x];
            bool canDraw = editType == PLAYER_PATH &&
                           (node->type == EMPTY ||
                            node->type == PLAYER_PATH);
            bool canErase = editType == EMPTY &&
                            node->type == PLAYER_PATH;
            if (node != startNode && node != endNode &&
                node->type != OBSTACLE && (canDraw || canErase)) {
                node->setType(editType);
            }
        }

        if (x == endX && y == endY) break;

        int doubledError = 2 * error;
        if (doubledError > -dy) {
            error -= dy;
            x += stepX;
        } else if (doubledError < dx) {
            error += dx;
            y += stepY;
        }
    }
}

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

static long long medianDuration(std::vector<long long> samples) {
    if (samples.empty()) return 0;
    std::sort(samples.begin(), samples.end());

    std::size_t middle = samples.size() / 2;
    if (samples.size() % 2 == 1) return samples[middle];
    return samples[middle - 1] +
           (samples[middle] - samples[middle - 1]) / 2;
}

BenchmarkMetrics benchmarkAlgorithms(
    std::vector<std::vector<GridNode*>>& grid,
    GridNode* start,
    GridNode* end,
    std::size_t warmupRuns,
    std::size_t measuredRuns)
{
    BenchmarkMetrics benchmark;
    benchmark.warmupRuns = warmupRuns;
    benchmark.measuredRuns = measuredRuns;
    if (measuredRuns == 0) return benchmark;

    Dijkstra dijkstra;
    AStar astar;

    for (std::size_t run = 0; run < warmupRuns; ++run) {
        dijkstra.solve(grid, start, end, false);
        astar.solve(grid, start, end, false);
    }

    std::vector<long long> dijkstraDurations;
    std::vector<long long> astarDurations;
    dijkstraDurations.reserve(measuredRuns);
    astarDurations.reserve(measuredRuns);

    auto measure = [&](Pathfinder& pathfinder) {
        auto started = std::chrono::steady_clock::now();
        pathfinder.solve(grid, start, end, false);
        auto finished = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            finished - started
        ).count();
    };

    // Alternate order so neither solver always benefits from running second.
    for (std::size_t run = 0; run < measuredRuns; ++run) {
        if (run % 2 == 0) {
            dijkstraDurations.push_back(measure(dijkstra));
            astarDurations.push_back(measure(astar));
        } else {
            astarDurations.push_back(measure(astar));
            dijkstraDurations.push_back(measure(dijkstra));
        }
    }

    benchmark.dijkstraMedianNanoseconds = medianDuration(dijkstraDurations);
    benchmark.astarMedianNanoseconds = medianDuration(astarDurations);
    benchmark.available = true;
    return benchmark;
}
