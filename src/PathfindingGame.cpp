#include "PathfindingGame.h"

// Helper structs for Priority Queue comparison
struct QueueEntry {
    double priority;
    GridNode* node;
};

struct QueueEntryCompare {
    bool operator()(const QueueEntry& a, const QueueEntry& b) const {
        return a.priority > b.priority; // Min-heap behavior
    }
};

void resetSolverState(std::vector<std::vector<GridNode*>>& grid,
                      std::vector<GridNode*>& nodesToAnimate) {
    for (auto& row : grid) {
        for (GridNode* node : row) {
            node->resetPathData();
        }
    }
    nodesToAnimate.clear();
}

// Helper to get valid neighbors (Up, Down, Left, Right)
std::vector<GridNode*> getNeighbors(GridNode* node,
                                    std::vector<std::vector<GridNode*>>& grid) {
    std::vector<GridNode*> neighbors;
    int x = node->x;
    int y = node->y;

    if (x > 0)              neighbors.push_back(grid[y][x - 1]);     // Left
    if (x < COL_COUNT - 1)  neighbors.push_back(grid[y][x + 1]);     // Right
    if (y > 0)              neighbors.push_back(grid[y - 1][x]);     // Up
    if (y < ROW_COUNT - 1)  neighbors.push_back(grid[y + 1][x]);     // Down

    return neighbors;
}

// --- Dijkstra Implementation ---
void Dijkstra::solve(std::vector<std::vector<GridNode*>>& grid,
                     GridNode* start,
                     GridNode* end,
                     std::vector<GridNode*>& nodesToAnimate) {
    resetSolverState(grid, nodesToAnimate);

    std::priority_queue<QueueEntry,
                        std::vector<QueueEntry>,
                        QueueEntryCompare> pq;

    start->gCost = 0.0;
    start->fCost = 0.0;
    start->parent = nullptr;
    pq.push({start->fCost, start});

    while (!pq.empty()) {
        QueueEntry entry = pq.top();
        pq.pop();
        GridNode* current = entry.node;

        if (entry.priority != current->fCost) continue;

        // Capture order for animation
        if (current != start && current != end) {
            nodesToAnimate.push_back(current);
        }

        if (current == end) {
            return; // Found target
        }

        for (GridNode* neighbor : getNeighbors(current, grid)) {
            if (neighbor->type == WALL || neighbor->type == OBSTACLE) continue;

            double tempG = current->gCost + 1.0; // Distance is 1 per cell

            if (tempG < neighbor->gCost) {
                neighbor->gCost = tempG;
                neighbor->fCost = tempG; // For Dijkstra, f = g
                neighbor->parent = current;
                pq.push({neighbor->fCost, neighbor});
            }
        }
    }
}

// --- A* Implementation ---
void AStar::solve(std::vector<std::vector<GridNode*>>& grid,
                  GridNode* start,
                  GridNode* end,
                  std::vector<GridNode*>& nodesToAnimate) {
    resetSolverState(grid, nodesToAnimate);

    std::priority_queue<QueueEntry,
                        std::vector<QueueEntry>,
                        QueueEntryCompare> pq;

    start->gCost = 0.0;
    start->hCost = std::abs(start->x - end->x)
                 + std::abs(start->y - end->y);
    start->fCost = start->gCost + start->hCost;
    start->parent = nullptr;
    pq.push({start->fCost, start});

    while (!pq.empty()) {
        QueueEntry entry = pq.top();
        pq.pop();
        GridNode* current = entry.node;

        if (entry.priority != current->fCost) continue;

        if (current != start && current != end) {
            nodesToAnimate.push_back(current);
        }

        if (current == end) {
            return;
        }

        for (GridNode* neighbor : getNeighbors(current, grid)) {
            if (neighbor->type == WALL || neighbor->type == OBSTACLE) continue;

            double tempG = current->gCost + 1.0;

            if (tempG < neighbor->gCost) {
                neighbor->gCost = tempG;
                // Manhattan Distance Heuristic: |x1 - x2| + |y1 - y2|
                neighbor->hCost = std::abs(neighbor->x - end->x)
                                + std::abs(neighbor->y - end->y);
                neighbor->fCost = neighbor->gCost + neighbor->hCost; // F = G + H
                neighbor->parent = current;
                pq.push({neighbor->fCost, neighbor});
            }
        }
    }
}
