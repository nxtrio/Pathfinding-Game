#include "MapGeneration.h"

#include <array>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr int CLASSIC_ROWS = 30;
constexpr int CLASSIC_COLS = 40;
constexpr GridPosition CLASSIC_START{5, 5};
constexpr GridPosition CLASSIC_END{25, 15};
constexpr int MINIMUM_MAZE_SIZE = 11;

struct FarthestCell {
    GridPosition position;
    int distance = 0;
};

enum DivisionOrientation { VERTICAL_DIVISION, HORIZONTAL_DIVISION };

bool samePosition(const GridPosition& left, const GridPosition& right) {
    return left == right;
}

void validateClassicConfig(const MapConfig& config) {
    if (config.rows != CLASSIC_ROWS || config.cols != CLASSIC_COLS ||
        !samePosition(config.start, CLASSIC_START) ||
        !samePosition(config.end, CLASSIC_END)) {
        throw std::invalid_argument(
            "Classic map dimensions and endpoints are fixed"
        );
    }
}

void validateProceduralDimensions(int rows, int cols) {
    if (rows < MINIMUM_MAZE_SIZE || cols < MINIMUM_MAZE_SIZE) {
        throw std::invalid_argument(
            "Procedural mazes must be at least 11 by 11"
        );
    }
    if (rows % 2 == 0 || cols % 2 == 0) {
        throw std::invalid_argument(
            "Procedural maze dimensions must be odd"
        );
    }
}

void validateProceduralGrid(const Grid& grid) {
    if (grid.empty() || grid.front().empty()) {
        throw std::invalid_argument("Procedural maze grid cannot be empty");
    }

    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid.front().size());
    validateProceduralDimensions(rows, cols);
    for (const auto& row : grid) {
        if (static_cast<int>(row.size()) != cols) {
            throw std::invalid_argument(
                "Procedural maze grid must be rectangular"
            );
        }
    }
}

std::size_t selectGrowingTreeCell(
    const std::vector<GridPosition>& activeCells
) {
    return activeCells.size() - 1;
}

DivisionOrientation chooseDivisionOrientation(int logicalWidth,
                                              int logicalHeight,
                                              std::mt19937& rng) {
    if (logicalWidth <= 1) return HORIZONTAL_DIVISION;
    if (logicalHeight <= 1) return VERTICAL_DIVISION;
    if (logicalWidth > logicalHeight) return VERTICAL_DIVISION;
    if (logicalHeight > logicalWidth) return HORIZONTAL_DIVISION;

    std::uniform_int_distribution<int> equalRegionChoice(0, 1);
    return equalRegionChoice(rng) == 0
        ? VERTICAL_DIVISION
        : HORIZONTAL_DIVISION;
}

void divideRegion(Grid& grid,
                  int minX,
                  int minY,
                  int maxX,
                  int maxY,
                  std::mt19937& rng) {
    int logicalWidth = (maxX - minX) / 2 + 1;
    int logicalHeight = (maxY - minY) / 2 + 1;
    if (logicalWidth <= 1 && logicalHeight <= 1) return;

    DivisionOrientation orientation = chooseDivisionOrientation(
        logicalWidth, logicalHeight, rng
    );

    if (orientation == VERTICAL_DIVISION) {
        std::uniform_int_distribution<int> wallChoice(0, logicalWidth - 2);
        std::uniform_int_distribution<int> gapChoice(0, logicalHeight - 1);
        int wallX = minX + 1 + 2 * wallChoice(rng);
        int gapY = minY + 2 * gapChoice(rng);

        for (int y = minY - 1; y <= maxY + 1; ++y) {
            grid[y][wallX]->setType(y == gapY ? EMPTY : OBSTACLE);
        }

        divideRegion(grid, minX, minY, wallX - 1, maxY, rng);
        divideRegion(grid, wallX + 1, minY, maxX, maxY, rng);
        return;
    }

    std::uniform_int_distribution<int> wallChoice(0, logicalHeight - 2);
    std::uniform_int_distribution<int> gapChoice(0, logicalWidth - 1);
    int wallY = minY + 1 + 2 * wallChoice(rng);
    int gapX = minX + 2 * gapChoice(rng);

    for (int x = minX - 1; x <= maxX + 1; ++x) {
        grid[wallY][x]->setType(x == gapX ? EMPTY : OBSTACLE);
    }

    divideRegion(grid, minX, minY, maxX, wallY - 1, rng);
    divideRegion(grid, minX, wallY + 1, maxX, maxY, rng);
}

bool isEndpointPassage(GridNode* node) {
    return node->type == EMPTY || node->type == START || node->type == END;
}

FarthestCell farthestReachableCell(const Grid& grid,
                                   GridPosition origin) {
    std::vector<std::vector<int>> distance;
    distance.reserve(grid.size());
    for (const auto& row : grid) {
        distance.emplace_back(row.size(), -1);
    }

    std::queue<GridPosition> frontier;
    distance[origin.y][origin.x] = 0;
    frontier.push(origin);
    FarthestCell farthest{origin, 0};

    constexpr std::array<GridPosition, 4> directions = {{
        {1, 0}, {0, 1}, {-1, 0}, {0, -1}
    }};

    while (!frontier.empty()) {
        GridPosition current = frontier.front();
        frontier.pop();
        int currentDistance = distance[current.y][current.x];
        if (currentDistance > farthest.distance) {
            farthest = {current, currentDistance};
        }

        for (GridPosition direction : directions) {
            int nextX = current.x + direction.x;
            int nextY = current.y + direction.y;
            if (nextY < 0 || nextY >= static_cast<int>(grid.size()) ||
                nextX < 0 ||
                nextX >= static_cast<int>(grid[nextY].size()) ||
                distance[nextY][nextX] != -1 ||
                !isEndpointPassage(grid[nextY][nextX])) {
                continue;
            }

            distance[nextY][nextX] = currentDistance + 1;
            frontier.push({nextX, nextY});
        }
    }

    return farthest;
}

void applyClassicObstacles(LoadedMap& map) {
    const int rows = static_cast<int>(map.grid.size());
    const int cols = static_cast<int>(map.grid.front().size());

    auto markColumnWithGap = [&](int col, int gapRow) {
        for (int y = 0; y < rows; ++y) {
            GridNode* node = map.grid[y][col];
            if (node == map.startNode || node == map.endNode || y == gapRow) {
                continue;
            }
            node->setType(OBSTACLE);
        }
    };

    markColumnWithGap(cols / 4, rows / 3);
    markColumnWithGap(cols / 2, rows / 2);
    markColumnWithGap(3 * cols / 4, 2 * rows / 3);
}

void assignDistantEndpoints(LoadedMap& map) {
    auto [start, end] = findDistantEndpoints(map.grid);
    map.config.start = start;
    map.config.end = end;
    map.startNode = map.grid[start.y][start.x];
    map.endNode = map.grid[end.y][end.x];
    map.startNode->setType(START);
    map.endNode->setType(END);
}

} // namespace

LoadedMap::~LoadedMap() {
    destroyGrid(grid);
}

LoadedMap::LoadedMap(LoadedMap&& other) noexcept
    : grid(std::move(other.grid)),
      startNode(other.startNode),
      endNode(other.endNode),
      config(std::move(other.config)) {
    other.grid.clear();
    other.startNode = nullptr;
    other.endNode = nullptr;
}

LoadedMap& LoadedMap::operator=(LoadedMap&& other) noexcept {
    if (this == &other) return *this;

    destroyGrid(grid);
    grid = std::move(other.grid);
    startNode = other.startNode;
    endNode = other.endNode;
    config = std::move(other.config);

    other.grid.clear();
    other.startNode = nullptr;
    other.endNode = nullptr;
    return *this;
}

MapConfig classicMapConfig() {
    return MapConfig{
        CLASSIC_MAP,
        "Classic",
        CLASSIC_ROWS,
        CLASSIC_COLS,
        0,
        CLASSIC_START,
        CLASSIC_END
    };
}

MapConfig growingTreeMapConfig(int rows,
                               int cols,
                               unsigned int seed) {
    return MapConfig{
        GROWING_TREE_MAZE,
        "Growing Tree Maze",
        rows,
        cols,
        seed,
        {0, 0},
        {0, 0}
    };
}

MapConfig recursiveDivisionMapConfig(int rows,
                                     int cols,
                                     unsigned int seed) {
    return MapConfig{
        RECURSIVE_DIVISION_MAZE,
        "Recursive Division Maze",
        rows,
        cols,
        seed,
        {0, 0},
        {0, 0}
    };
}

void generateGrowingTreeMaze(Grid& grid, unsigned int seed) {
    validateProceduralGrid(grid);
    const int rows = static_cast<int>(grid.size());
    const int cols = static_cast<int>(grid.front().size());
    const int logicalRows = (rows - 1) / 2;
    const int logicalCols = (cols - 1) / 2;

    for (auto& row : grid) {
        for (GridNode* node : row) {
            node->setType(OBSTACLE);
            node->resetPathData();
        }
    }

    std::vector<std::vector<bool>> visited(
        logicalRows, std::vector<bool>(logicalCols, false)
    );
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> startRowDistribution(
        0, logicalRows - 1
    );
    std::uniform_int_distribution<int> startColDistribution(
        0, logicalCols - 1
    );

    int startLogicalY = startRowDistribution(rng);
    int startLogicalX = startColDistribution(rng);
    GridPosition start{
        2 * startLogicalX + 1,
        2 * startLogicalY + 1
    };
    visited[startLogicalY][startLogicalX] = true;
    grid[start.y][start.x]->setType(EMPTY);

    std::vector<GridPosition> activeCells{start};
    constexpr std::array<GridPosition, 4> directions = {{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    }};

    while (!activeCells.empty()) {
        std::size_t selectedIndex = selectGrowingTreeCell(activeCells);
        GridPosition current = activeCells[selectedIndex];
        std::vector<GridPosition> candidates;
        candidates.reserve(directions.size());

        for (GridPosition direction : directions) {
            GridPosition next{
                current.x + 2 * direction.x,
                current.y + 2 * direction.y
            };
            if (next.x <= 0 || next.x >= cols - 1 ||
                next.y <= 0 || next.y >= rows - 1) {
                continue;
            }

            int logicalX = (next.x - 1) / 2;
            int logicalY = (next.y - 1) / 2;
            if (!visited[logicalY][logicalX]) candidates.push_back(next);
        }

        if (candidates.empty()) {
            if (selectedIndex == activeCells.size() - 1) {
                activeCells.pop_back();
            } else {
                activeCells.erase(activeCells.begin() + selectedIndex);
            }
            continue;
        }

        std::uniform_int_distribution<std::size_t> candidateDistribution(
            0, candidates.size() - 1
        );
        GridPosition next = candidates[candidateDistribution(rng)];
        GridPosition wall{
            (current.x + next.x) / 2,
            (current.y + next.y) / 2
        };

        grid[wall.y][wall.x]->setType(EMPTY);
        grid[next.y][next.x]->setType(EMPTY);
        visited[(next.y - 1) / 2][(next.x - 1) / 2] = true;
        activeCells.push_back(next);
    }
}

void generateRecursiveDivisionMaze(Grid& grid, unsigned int seed) {
    validateProceduralGrid(grid);
    const int rows = static_cast<int>(grid.size());
    const int cols = static_cast<int>(grid.front().size());

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            GridNode* node = grid[y][x];
            bool boundary = y == 0 || y == rows - 1 ||
                            x == 0 || x == cols - 1;
            node->setType(boundary ? OBSTACLE : EMPTY);
            node->resetPathData();
        }
    }

    std::mt19937 rng(seed);
    divideRegion(grid, 1, 1, cols - 2, rows - 2, rng);
}

std::pair<GridPosition, GridPosition> findDistantEndpoints(
    const Grid& grid
) {
    GridPosition firstPassage{-1, -1};
    for (int y = 0; y < static_cast<int>(grid.size()) &&
                    firstPassage.x < 0; ++y) {
        for (int x = 0; x < static_cast<int>(grid[y].size()); ++x) {
            if (isEndpointPassage(grid[y][x])) {
                firstPassage = {x, y};
                break;
            }
        }
    }

    if (firstPassage.x < 0) {
        throw std::invalid_argument(
            "Cannot choose endpoints on a map without passages"
        );
    }

    FarthestCell first = farthestReachableCell(grid, firstPassage);
    FarthestCell second = farthestReachableCell(grid, first.position);
    if (second.distance == 0) {
        throw std::invalid_argument(
            "Cannot choose distinct endpoints on a single-cell map"
        );
    }
    return {first.position, second.position};
}

LoadedMap createMap(const MapConfig& config) {
    LoadedMap map;
    map.config = config;

    if (config.type == CLASSIC_MAP) {
        validateClassicConfig(config);
        if (map.config.name.empty()) map.config.name = "Classic";
        map.grid = createGrid(config.rows, config.cols);
        map.startNode = map.grid[config.start.y][config.start.x];
        map.endNode = map.grid[config.end.y][config.end.x];
        map.startNode->setType(START);
        map.endNode->setType(END);
        applyClassicObstacles(map);
        return map;
    }

    if (config.type == GROWING_TREE_MAZE) {
        validateProceduralDimensions(config.rows, config.cols);
        if (map.config.name.empty()) map.config.name = "Growing Tree Maze";
        map.grid = createGrid(config.rows, config.cols);
        generateGrowingTreeMaze(map.grid, config.seed);
        assignDistantEndpoints(map);
        return map;
    }

    if (config.type == RECURSIVE_DIVISION_MAZE) {
        validateProceduralDimensions(config.rows, config.cols);
        if (map.config.name.empty()) {
            map.config.name = "Recursive Division Maze";
        }
        map.grid = createGrid(config.rows, config.cols);
        generateRecursiveDivisionMaze(map.grid, config.seed);
        assignDistantEndpoints(map);
        return map;
    }

    throw std::invalid_argument(
        "Requested procedural map generator is not implemented yet"
    );
}
