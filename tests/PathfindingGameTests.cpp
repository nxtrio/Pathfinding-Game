#include "PathfindingGame.h"
#include "PathfindingAnimation.h"
#include "ComparisonUI.h"
#include "MapGeneration.h"
#include "MapSelectionUI.h"

#include <cmath>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class TestGrid {
public:
    Grid nodes;

    TestGrid(int rows, int cols)
        : nodes(createGrid(rows, cols)) {}

    ~TestGrid() {
        destroyGrid(nodes);
    }

    TestGrid(const TestGrid&) = delete;
    TestGrid& operator=(const TestGrid&) = delete;
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int positionKey(const GridPosition& position, int cols) {
    return position.y * cols + position.x;
}

void validatePath(const Grid& grid,
                  GridNode* start,
                  GridNode* end,
                  const SearchResult& result) {
    require(result.metrics.found, "expected a path to be found");
    require(!result.path.empty(), "found result must contain a path");
    require(result.path.front() == GridPosition{start->x, start->y},
            "path must begin at start");
    require(result.path.back() == GridPosition{end->x, end->y},
            "path must end at target");
    require(result.metrics.pathLength == static_cast<int>(result.path.size()) - 1,
            "path length must count edges");

    for (std::size_t i = 0; i < result.path.size(); ++i) {
        const GridPosition& position = result.path[i];
        require(position.y >= 0 &&
                position.y < static_cast<int>(grid.size()),
                "path row is outside the grid");
        require(position.x >= 0 &&
                position.x < static_cast<int>(grid[position.y].size()),
                "path column is outside the grid");
        require(grid[position.y][position.x]->type != OBSTACLE,
                "path must not enter an obstacle");

        if (i > 0) {
            const GridPosition& previous = result.path[i - 1];
            int distance = std::abs(position.x - previous.x)
                         + std::abs(position.y - previous.y);
            require(distance == 1, "path steps must be four-directionally adjacent");
        }
    }
}

void validateMetrics(const SearchResult& result) {
    require(result.metrics.discoveredNodes >= result.metrics.expandedNodes,
            "discovered count must be at least expanded count");
    require(result.metrics.maxFrontierSize > 0,
            "nontrivial search must report a frontier");
    require(result.metrics.maxFrontierSize <= result.metrics.discoveredNodes,
            "logical frontier cannot exceed unique discovered nodes");
    require(result.metrics.singleRunMicroseconds >= 0,
            "runtime measurement cannot be negative");
    if (result.metrics.found) {
        require(result.metrics.pathLength >= 0,
                "found search must report a non-negative path length");
    }
}

void validateTrace(const SearchResult& result,
                   GridNode* start,
                   GridNode* end,
                   int cols) {
    std::set<int> discoveredPositions;
    std::set<int> expandedPositions;

    for (const SearchStep& step : result.steps) {
        require(!(step.position == GridPosition{start->x, start->y}),
                "trace must not include start");
        require(!(step.position == GridPosition{end->x, end->y}),
                "trace must not include target");

        int key = positionKey(step.position, cols);
        if (step.type == DISCOVERED) {
            discoveredPositions.insert(key);
        } else {
            require(discoveredPositions.count(key) == 1,
                    "node must be discovered before it is expanded");
            require(expandedPositions.insert(key).second,
                    "node must not be expanded more than once");
        }
    }

    require(result.metrics.discoveredNodes == discoveredPositions.size() + 2,
            "discovered metric must count unique trace nodes plus start and target");
    require(result.metrics.expandedNodes == expandedPositions.size() + 1,
            "expanded metric must count unique trace nodes plus start");
}

bool stableResultMatches(const SearchResult& a, const SearchResult& b) {
    if (a.metrics.found != b.metrics.found ||
        a.metrics.pathLength != b.metrics.pathLength ||
        a.metrics.discoveredNodes != b.metrics.discoveredNodes ||
        a.metrics.expandedNodes != b.metrics.expandedNodes ||
        a.metrics.maxFrontierSize != b.metrics.maxFrontierSize ||
        a.path != b.path || a.steps.size() != b.steps.size()) {
        return false;
    }

    for (std::size_t i = 0; i < a.steps.size(); ++i) {
        if (!(a.steps[i].position == b.steps[i].position) ||
            a.steps[i].type != b.steps[i].type) {
            return false;
        }
    }
    return true;
}

std::vector<NodeType> captureNodeTypes(const Grid& grid) {
    std::vector<NodeType> types;
    for (const auto& row : grid) {
        for (GridNode* node : row) types.push_back(node->type);
    }
    return types;
}

bool obstacleLayoutsMatch(const Grid& left, const Grid& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t y = 0; y < left.size(); ++y) {
        if (left[y].size() != right[y].size()) return false;
        for (std::size_t x = 0; x < left[y].size(); ++x) {
            if ((left[y][x]->type == OBSTACLE) !=
                (right[y][x]->type == OBSTACLE)) {
                return false;
            }
        }
    }
    return true;
}

std::size_t reachablePassageCount(const Grid& grid, GridPosition start) {
    std::vector<std::vector<bool>> visited;
    visited.reserve(grid.size());
    for (const auto& row : grid) {
        visited.emplace_back(row.size(), false);
    }

    std::queue<GridPosition> frontier;
    frontier.push(start);
    visited[start.y][start.x] = true;
    std::size_t reachable = 0;
    constexpr int DX[4] = {1, -1, 0, 0};
    constexpr int DY[4] = {0, 0, 1, -1};

    while (!frontier.empty()) {
        GridPosition current = frontier.front();
        frontier.pop();
        ++reachable;

        for (int direction = 0; direction < 4; ++direction) {
            int nextX = current.x + DX[direction];
            int nextY = current.y + DY[direction];
            if (nextY < 0 || nextY >= static_cast<int>(grid.size()) ||
                nextX < 0 ||
                nextX >= static_cast<int>(grid[nextY].size()) ||
                visited[nextY][nextX] ||
                grid[nextY][nextX]->type == OBSTACLE) {
                continue;
            }
            visited[nextY][nextX] = true;
            frontier.push({nextX, nextY});
        }
    }

    return reachable;
}

void validateGrowingTreeMap(const LoadedMap& map) {
    const int rows = map.config.rows;
    const int cols = map.config.cols;
    require(rows >= 11 && cols >= 11 && rows % 2 == 1 && cols % 2 == 1,
            "Growing Tree maps must retain valid odd dimensions");
    require(map.startNode != nullptr && map.endNode != nullptr &&
            map.startNode != map.endNode &&
            map.startNode->type == START && map.endNode->type == END,
            "Growing Tree maps must contain distinct endpoints");
    require(map.startNode->x > 0 && map.startNode->x < cols - 1 &&
            map.startNode->y > 0 && map.startNode->y < rows - 1 &&
            map.endNode->x > 0 && map.endNode->x < cols - 1 &&
            map.endNode->y > 0 && map.endNode->y < rows - 1,
            "Growing Tree endpoints must remain inside the obstacle border");
    require(map.startNode->x % 2 == 1 && map.startNode->y % 2 == 1 &&
            map.endNode->x % 2 == 1 && map.endNode->y % 2 == 1,
            "Growing Tree endpoints must lie on logical maze cells");

    std::size_t passageCount = 0;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            NodeType type = map.grid[y][x]->type;
            require(type == EMPTY || type == OBSTACLE ||
                    type == START || type == END,
                    "fresh Growing Tree maps must use only permanent types");
            if (y == 0 || y == rows - 1 || x == 0 || x == cols - 1) {
                require(type == OBSTACLE,
                        "Growing Tree maps must preserve an obstacle border");
            }
            if (type != OBSTACLE) {
                require(!(x % 2 == 0 && y % 2 == 0),
                        "Growing Tree must not carve even/even intersections");
                ++passageCount;
            }
        }
    }

    std::size_t logicalCells = static_cast<std::size_t>((rows - 1) / 2) *
                               static_cast<std::size_t>((cols - 1) / 2);
    require(passageCount == 2 * logicalCells - 1,
            "Growing Tree newest-cell carving must produce a perfect maze");
    require(reachablePassageCount(
                map.grid, {map.startNode->x, map.startNode->y}
            ) == passageCount,
            "every Growing Tree passage must be connected");
}

void validateRecursiveDivisionMap(const LoadedMap& map) {
    const int rows = map.config.rows;
    const int cols = map.config.cols;
    require(rows >= 11 && cols >= 11 && rows % 2 == 1 && cols % 2 == 1,
            "Recursive Division maps must retain valid odd dimensions");
    require(map.startNode != nullptr && map.endNode != nullptr &&
            map.startNode != map.endNode &&
            map.startNode->type == START && map.endNode->type == END,
            "Recursive Division maps must contain distinct endpoints");
    require(map.startNode->x % 2 == 1 && map.startNode->y % 2 == 1 &&
            map.endNode->x % 2 == 1 && map.endNode->y % 2 == 1,
            "Recursive Division endpoints must lie on logical maze cells");

    std::size_t passageCount = 0;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            NodeType type = map.grid[y][x]->type;
            require(type == EMPTY || type == OBSTACLE ||
                    type == START || type == END,
                    "fresh Recursive Division maps must use permanent types");
            if (y == 0 || y == rows - 1 || x == 0 || x == cols - 1) {
                require(type == OBSTACLE,
                        "Recursive Division must preserve an obstacle border");
            }
            if (x % 2 == 1 && y % 2 == 1) {
                require(type != OBSTACLE,
                        "Recursive Division walls must use even coordinates");
            }
            if (type != OBSTACLE) {
                require(!(x % 2 == 0 && y % 2 == 0),
                        "Recursive Division gaps must use odd coordinates");
                ++passageCount;
            }
        }
    }

    std::size_t logicalCells = static_cast<std::size_t>((rows - 1) / 2) *
                               static_cast<std::size_t>((cols - 1) / 2);
    require(passageCount == 2 * logicalCells - 1,
            "Recursive Division must produce a perfect maze raster");
    require(reachablePassageCount(
                map.grid, {map.startNode->x, map.startNode->y}
            ) == passageCount,
            "every Recursive Division passage must be connected");
}

void testClassicMapModelAndReplacement() {
    MapConfig config = classicMapConfig();
    require(config.type == CLASSIC_MAP && config.name == "Classic" &&
            config.rows == 30 && config.cols == 40 && config.seed == 0 &&
            config.start == GridPosition{5, 5} &&
            config.end == GridPosition{25, 15},
            "Classic config must preserve original map metadata");

    LoadedMap map = createMap(config);
    require(map.grid.size() == 30 && map.grid.front().size() == 40,
            "Classic factory must construct the original dimensions");
    require(map.startNode == map.grid[5][5] &&
            map.endNode == map.grid[15][25] &&
            map.startNode->type == START && map.endNode->type == END,
            "Classic factory must bind the original endpoints");

    std::size_t obstacleCount = 0;
    for (int y = 0; y < config.rows; ++y) {
        for (int x = 0; x < config.cols; ++x) {
            bool classicBarrier =
                (x == 10 && y != 10) ||
                (x == 20 && y != 15) ||
                (x == 30 && y != 20);
            GridNode* node = map.grid[y][x];
            if (classicBarrier) {
                require(node->type == OBSTACLE,
                        "Classic barrier layout must remain unchanged");
                ++obstacleCount;
            } else if (node != map.startNode && node != map.endNode) {
                require(node->type == EMPTY,
                        "Classic non-barrier cells must begin empty");
            }
        }
    }
    require(obstacleCount == 87,
            "Classic map must retain three 29-cell obstacle columns");

    map.grid[5][6]->setType(PLAYER_PATH);
    LoadedMap replacement = createMap(config);
    GridNode* replacementStart = replacement.startNode;
    map = std::move(replacement);
    require(map.startNode == replacementStart &&
            map.grid[5][6]->type == EMPTY && replacement.grid.empty() &&
            replacement.startNode == nullptr && replacement.endNode == nullptr,
            "map replacement must transfer ownership and fresh logical state");

    MapConfig invalidClassic = config;
    invalidClassic.rows = 31;
    bool invalidClassicRejected = false;
    try {
        createMap(invalidClassic);
    } catch (const std::invalid_argument&) {
        invalidClassicRejected = true;
    }
    require(invalidClassicRejected,
            "Classic factory must reject dimensions that change its identity");
}

void testMapSelectionModelAndActions() {
    require(dimensionsForPreset(SMALL_MAP_SIZE).rows == 21 &&
            dimensionsForPreset(SMALL_MAP_SIZE).cols == 31 &&
            dimensionsForPreset(MEDIUM_MAP_SIZE).rows == 35 &&
            dimensionsForPreset(MEDIUM_MAP_SIZE).cols == 51 &&
            dimensionsForPreset(LARGE_MAP_SIZE).rows == 55 &&
            dimensionsForPreset(LARGE_MAP_SIZE).cols == 81,
            "map selector must expose all three odd-sized maze presets");

    MapSelectionState selection;
    MapConfig config = mapConfigForSelection(selection);
    require(config.type == CLASSIC_MAP && config.rows == 30 &&
            config.cols == 40 && config.seed == 0,
            "default selection must launch the original Classic map");

    selection.selectedType = GROWING_TREE_MAZE;
    selection.selectedSize = SMALL_MAP_SIZE;
    selection.seed = 12345u;
    config = mapConfigForSelection(selection);
    require(config.type == GROWING_TREE_MAZE && config.rows == 21 &&
            config.cols == 31 && config.seed == 12345u,
            "Growing Tree selection must preserve preset and displayed seed");

    selection.selectedType = RECURSIVE_DIVISION_MAZE;
    selection.selectedSize = LARGE_MAP_SIZE;
    config = mapConfigForSelection(selection);
    require(config.type == RECURSIVE_DIVISION_MAZE && config.rows == 55 &&
            config.cols == 81 && config.seed == 12345u,
            "Recursive Division selection must preserve preset and seed");

    const std::pair<sf::Vector2f, MapSelectionAction> actions[] = {
        {{100.f, 200.f}, SELECT_CLASSIC_ACTION},
        {{500.f, 200.f}, SELECT_GROWING_TREE_ACTION},
        {{900.f, 200.f}, SELECT_RECURSIVE_DIVISION_ACTION},
        {{460.f, 480.f}, SELECT_SMALL_SIZE_ACTION},
        {{605.f, 480.f}, SELECT_MEDIUM_SIZE_ACTION},
        {{750.f, 480.f}, SELECT_LARGE_SIZE_ACTION},
        {{500.f, 580.f}, RANDOMIZE_SEED_ACTION},
        {{700.f, 580.f}, START_MAP_ACTION},
        {{100.f, 700.f}, NO_MAP_SELECTION_ACTION}
    };
    for (const auto& [position, expected] : actions) {
        require(mapSelectionActionAt(position) == expected,
                "map selector click region returned the wrong action");
    }
}

void testGrowingTreeMazeGeneration() {
    const std::pair<int, int> sizes[] = {
        {11, 11}, {21, 31}, {35, 51}, {55, 81}
    };

    for (const auto& [rows, cols] : sizes) {
        MapConfig config = growingTreeMapConfig(rows, cols, 482917u);
        require(config.type == GROWING_TREE_MAZE &&
                config.name == "Growing Tree Maze" &&
                config.rows == rows && config.cols == cols &&
                config.seed == 482917u,
                "Growing Tree config must retain dimensions and seed");

        LoadedMap map = createMap(config);
        validateGrowingTreeMap(map);
        auto selectedEndpoints = findDistantEndpoints(map.grid);
        require(map.config.start == GridPosition{
                    map.startNode->x, map.startNode->y
                } &&
                map.config.end == GridPosition{
                    map.endNode->x, map.endNode->y
                } &&
                selectedEndpoints.first == map.config.start &&
                selectedEndpoints.second == map.config.end,
                "generated endpoint metadata must match loaded nodes");

        AlgorithmComparison comparison = runAlgorithmComparison(
            map.grid, map.startNode, map.endNode, false
        );
        require(comparison.status == MATCHING_PATHS &&
                comparison.dijkstra.metrics.found &&
                comparison.astar.metrics.found &&
                comparison.dijkstra.metrics.pathLength ==
                    comparison.astar.metrics.pathLength,
                "both solvers must agree on every Growing Tree preset");
    }

    MapConfig deterministicConfig = growingTreeMapConfig(21, 31, 12345u);
    LoadedMap first = createMap(deterministicConfig);
    LoadedMap second = createMap(deterministicConfig);
    require(captureNodeTypes(first.grid) == captureNodeTypes(second.grid),
            "same Growing Tree dimensions and seed must reproduce all cells");
    require(first.config.start == second.config.start &&
            first.config.end == second.config.end,
            "same Growing Tree seed must reproduce endpoint placement");

    LoadedMap varied = createMap(growingTreeMapConfig(21, 31, 54321u));
    require(!obstacleLayoutsMatch(first.grid, varied.grid),
            "different Growing Tree seeds should vary the obstacle layout");

    for (MapConfig invalid : {
            growingTreeMapConfig(9, 11, 1u),
            growingTreeMapConfig(20, 31, 1u),
            growingTreeMapConfig(21, 30, 1u)}) {
        bool rejected = false;
        try {
            createMap(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected,
                "Growing Tree factory must reject small or even dimensions");
    }
}

void testRecursiveDivisionMazeGeneration() {
    const std::pair<int, int> sizes[] = {
        {11, 11}, {21, 31}, {31, 21}, {35, 51}, {55, 81}
    };

    for (const auto& [rows, cols] : sizes) {
        MapConfig config = recursiveDivisionMapConfig(rows, cols, 482917u);
        require(config.type == RECURSIVE_DIVISION_MAZE &&
                config.name == "Recursive Division Maze" &&
                config.rows == rows && config.cols == cols &&
                config.seed == 482917u,
                "Recursive Division config must retain dimensions and seed");

        LoadedMap map = createMap(config);
        validateRecursiveDivisionMap(map);
        auto selectedEndpoints = findDistantEndpoints(map.grid);
        require(selectedEndpoints.first == map.config.start &&
                selectedEndpoints.second == map.config.end,
                "Recursive Division metadata must retain distant endpoints");

        AlgorithmComparison comparison = runAlgorithmComparison(
            map.grid, map.startNode, map.endNode, false
        );
        require(comparison.status == MATCHING_PATHS &&
                comparison.dijkstra.metrics.found &&
                comparison.astar.metrics.found &&
                comparison.dijkstra.metrics.pathLength ==
                    comparison.astar.metrics.pathLength,
                "both solvers must agree on every Recursive Division preset");
    }

    MapConfig deterministicConfig = recursiveDivisionMapConfig(
        21, 31, 12345u
    );
    LoadedMap first = createMap(deterministicConfig);
    LoadedMap second = createMap(deterministicConfig);
    require(captureNodeTypes(first.grid) == captureNodeTypes(second.grid) &&
            first.config.start == second.config.start &&
            first.config.end == second.config.end,
            "same Recursive Division seed must reproduce map and endpoints");

    LoadedMap varied = createMap(
        recursiveDivisionMapConfig(21, 31, 54321u)
    );
    require(!obstacleLayoutsMatch(first.grid, varied.grid),
            "different Recursive Division seeds should vary the layout");

    LoadedMap growingTree = createMap(
        growingTreeMapConfig(21, 31, 12345u)
    );
    require(!obstacleLayoutsMatch(first.grid, growingTree.grid),
            "Recursive Division must be structurally distinct from Growing Tree");

    for (MapConfig invalid : {
            recursiveDivisionMapConfig(9, 11, 1u),
            recursiveDivisionMapConfig(20, 31, 1u),
            recursiveDivisionMapConfig(21, 30, 1u)}) {
        bool rejected = false;
        try {
            createMap(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected,
                "Recursive Division must reject small or even dimensions");
    }
}

void testStandardMapOptimalityAndMetrics() {
    LoadedMap map = createMap(classicMapConfig());
    Grid& grid = map.grid;
    GridNode* start = map.startNode;
    GridNode* end = map.endNode;
    grid[5][6]->setType(PLAYER_PATH);

    std::vector<NodeType> originalTypes;
    for (const auto& row : grid) {
        for (GridNode* node : row) {
            originalTypes.push_back(node->type);
        }
    }

    AlgorithmComparison comparison = beginAlgorithmComparison(
        grid, start, end
    );
    require(!comparison.available,
            "comparison must wait for A* before reporting completion");
    require(comparison.dijkstra.metrics.found && comparison.astar.path.empty(),
            "staged comparison must retain Dijkstra before running A*");
    completeAlgorithmComparison(comparison, grid, start, end);
    const SearchResult& dijkstraResult = comparison.dijkstra;
    const SearchResult& astarResult = comparison.astar;

    require(comparison.available, "comparison must retain both algorithm results");
    require(comparison.status == MATCHING_PATHS,
            "standard-map comparison must report matching optimal paths");
    validatePath(grid, start, end, dijkstraResult);
    validatePath(grid, start, end, astarResult);
    validateMetrics(dijkstraResult);
    validateMetrics(astarResult);
    validateTrace(dijkstraResult, start, end, map.config.cols);
    validateTrace(astarResult, start, end, map.config.cols);
    require(dijkstraResult.metrics.pathLength == astarResult.metrics.pathLength,
            "Dijkstra and A* must find the same optimal length");
    require(start->hCost == 30.0,
            "A* must initialize the start Manhattan heuristic");

    std::size_t typeIndex = 0;
    for (const auto& row : grid) {
        for (GridNode* node : row) {
            require(node->type == originalTypes[typeIndex++],
                    "comparison must not mutate logical board state");
        }
    }
}

void testDynamicGridLifetimeEditingAndHandRoute() {
    TestGrid grid(35, 51);
    require(grid.nodes.size() == 35 && grid.nodes.front().size() == 51,
            "grid construction must honor requested dimensions");
    require(grid.nodes[34][50]->x == 50 && grid.nodes[34][50]->y == 34,
            "dynamic grid nodes must retain matching coordinates");

    std::optional<GridPosition> mappedPosition = worldToGridPosition(
        grid.nodes, sf::Vector2f(1262.5f, 862.5f)
    );
    require(mappedPosition.has_value() &&
            *mappedPosition == GridPosition{50, 34},
            "world coordinates must map to the correct dynamic-grid cell");
    require(!worldToGridPosition(
                grid.nodes, sf::Vector2f(-0.1f, 12.5f)
            ).has_value() &&
            !worldToGridPosition(
                grid.nodes, sf::Vector2f(1275.f, 862.5f)
            ).has_value(),
            "world-to-grid conversion must reject positions outside the map");

    GridNode* start = grid.nodes[34][0];
    GridNode* end = grid.nodes[34][50];
    start->setType(START);
    end->setType(END);

    editGridLine(grid.nodes, start, end, 0, 34, 50, 34, PLAYER_PATH);
    require(grid.nodes[34][45]->type == PLAYER_PATH,
            "line editing must reach columns beyond the Classic width");
    require(computeHandDrawnPathLength(grid.nodes, start, end) == 50,
            "hand-route BFS must traverse a non-Classic grid width");

    AlgorithmComparison comparison = runAlgorithmComparison(
        grid.nodes, start, end, false
    );
    require(comparison.status == MATCHING_PATHS &&
            comparison.dijkstra.metrics.pathLength == 50 &&
            comparison.astar.metrics.pathLength == 50,
            "both solvers must remain optimal on a non-Classic grid width");

    editGridLine(grid.nodes, start, end, 20, 34, 30, 34, EMPTY);
    require(computeHandDrawnPathLength(grid.nodes, start, end) == -1,
            "erasing a dynamic-grid route must break connectivity");

    editGridLine(grid.nodes, start, end, -5, 34, 55, 34, PLAYER_PATH);
    require(computeHandDrawnPathLength(grid.nodes, start, end) == 50,
            "out-of-bounds drag endpoints must clip safely to the grid");

    grid.nodes[34][25]->setType(OBSTACLE);
    editGridLine(grid.nodes, start, end, 24, 34, 26, 34, PLAYER_PATH);
    require(grid.nodes[34][25]->type == OBSTACLE &&
            computeHandDrawnPathLength(grid.nodes, start, end) == -1,
            "editing must preserve obstacles on dynamic grids");

    GridNode* originalFirstNode = grid.nodes.front().front();
    bool invalidSizeRejected = false;
    try {
        recreateGrid(grid.nodes, 0, 31);
    } catch (const std::invalid_argument&) {
        invalidSizeRejected = true;
    }
    require(invalidSizeRejected &&
            grid.nodes.size() == 35 && grid.nodes.front().size() == 51 &&
            grid.nodes.front().front() == originalFirstNode,
            "failed grid recreation must preserve the active grid");

    recreateGrid(grid.nodes, 21, 31);
    require(grid.nodes.size() == 21 && grid.nodes.front().size() == 31,
            "grid recreation must replace the active dimensions");
    require(grid.nodes[20][30]->x == 30 && grid.nodes[20][30]->y == 20 &&
            grid.nodes[20][30]->type == EMPTY,
            "recreated grids must contain fresh correctly positioned nodes");
}

void testKnownPathLengthAndPlayerPathTraversability() {
    TestGrid grid(1, 5);
    GridNode* start = grid.nodes[0][0];
    GridNode* end = grid.nodes[0][4];
    start->setType(START);
    end->setType(END);
    for (int x = 1; x < 4; ++x) {
        grid.nodes[0][x]->setType(PLAYER_PATH);
    }

    AlgorithmComparison comparison = runAlgorithmComparison(
        grid.nodes, start, end, false
    );
    const SearchResult& dijkstraResult = comparison.dijkstra;
    const SearchResult& astarResult = comparison.astar;

    require(comparison.status == MATCHING_PATHS,
            "player-path comparison must report matching optimal paths");
    validatePath(grid.nodes, start, end, dijkstraResult);
    validatePath(grid.nodes, start, end, astarResult);
    require(dijkstraResult.metrics.pathLength == 4,
            "known Dijkstra path length must be four edges");
    require(astarResult.metrics.pathLength == 4,
            "known A* path length must be four edges");
    require(dijkstraResult.steps.empty() && astarResult.steps.empty(),
            "captureSteps=false must omit trace events");
    for (int x = 1; x < 4; ++x) {
        require(grid.nodes[0][x]->type == PLAYER_PATH,
                "search reset must preserve player path semantics");
    }
}

void testRepeatedDeterministicRuns() {
    TestGrid grid(8, 8);
    GridNode* start = grid.nodes[1][1];
    GridNode* end = grid.nodes[6][6];
    start->setType(START);
    end->setType(END);
    for (int y = 0; y < 7; ++y) {
        if (y == 4) continue;
        grid.nodes[y][3]->setType(OBSTACLE);
    }

    AlgorithmComparison firstComparison = runAlgorithmComparison(
        grid.nodes, start, end
    );
    AlgorithmComparison secondComparison = runAlgorithmComparison(
        grid.nodes, start, end
    );
    const SearchResult& firstDijkstra = firstComparison.dijkstra;
    const SearchResult& firstAStar = firstComparison.astar;
    const SearchResult& secondDijkstra = secondComparison.dijkstra;
    const SearchResult& secondAStar = secondComparison.astar;

    require(firstComparison.status == MATCHING_PATHS &&
            secondComparison.status == MATCHING_PATHS,
            "repeated comparisons must report matching optimal paths");
    require(stableResultMatches(firstDijkstra, secondDijkstra),
            "repeated Dijkstra runs must be deterministic and independent");
    require(stableResultMatches(firstAStar, secondAStar),
            "repeated A* runs must be deterministic and independent");
    require(firstDijkstra.metrics.pathLength == firstAStar.metrics.pathLength,
            "repeated algorithms must agree on optimal length");
}

void testNoPathAndStaleParentReset() {
    TestGrid grid(3, 3);
    GridNode* start = grid.nodes[1][0];
    GridNode* end = grid.nodes[1][2];
    start->setType(START);
    end->setType(END);

    Dijkstra dijkstra;
    SearchResult openResult = dijkstra.solve(grid.nodes, start, end);
    require(openResult.metrics.found, "open board must initially have a route");
    require(end->parent != nullptr, "successful search must set target parent");

    for (int y = 0; y < 3; ++y) {
        grid.nodes[y][1]->setType(OBSTACLE);
    }

    AlgorithmComparison comparison = runAlgorithmComparison(
        grid.nodes, start, end
    );
    const SearchResult& astarResult = comparison.astar;
    const SearchResult& dijkstraResult = comparison.dijkstra;

    require(comparison.available, "no-path comparison must retain both results");
    require(comparison.status == BOTH_NO_PATH,
            "blocked comparison must distinguish shared no-path outcome");
    for (const SearchResult* result : {&astarResult, &dijkstraResult}) {
        require(!result->metrics.found, "blocked board must report no path");
        require(result->metrics.pathLength == -1,
                "no-path length must remain -1");
        require(result->path.empty(), "no-path result must have no path coordinates");
        validateMetrics(*result);
    }
    require(end->parent == nullptr,
            "failed repeated search must not retain a stale target parent");
}

void testAnimationReplayAndLogicalStateSafety() {
    TestGrid grid(1, 3);
    GridNode* start = grid.nodes[0][0];
    GridNode* playerPath = grid.nodes[0][1];
    GridNode* end = grid.nodes[0][2];
    start->setType(START);
    playerPath->setType(PLAYER_PATH);
    end->setType(END);

    sf::Color startColor = start->shape.getFillColor();
    sf::Color playerPathColor = playerPath->shape.getFillColor();
    sf::Color endColor = end->shape.getFillColor();

    SearchResult result;
    result.steps = {
        {{1, 0}, DISCOVERED},
        {{1, 0}, EXPANDED}
    };
    result.path = {{0, 0}, {1, 0}, {2, 0}};
    result.metrics.found = true;
    result.metrics.pathLength = 2;

    AnimationController animation;
    require(isAnimationState(ANIMATING_DIJKSTRA) &&
            isAnimationState(HOLDING_ASTAR) &&
            !isAnimationState(EDITING),
            "animation-state classification must be explicit");

    require(!updateSearchAnimation(
                grid.nodes, result, true, animation, 0.001f),
            "sub-interval update must not finish replay");
    require(animation.searchStepIndex == 0,
            "sub-interval update must not consume a search event");

    updateSearchAnimation(grid.nodes, result, true, animation, 0.002f);
    require(playerPath->shape.getFillColor() == sf::Color(0, 220, 255),
            "Dijkstra discovery must use frontier color");

    updateSearchAnimation(grid.nodes, result, true, animation, 0.0025f);
    require(playerPath->shape.getFillColor() == sf::Color(40, 120, 210),
            "Dijkstra expansion must replace frontier color");

    updateSearchAnimation(grid.nodes, result, true, animation, 0.f);
    require(animation.stage == REVEALING_PATH,
            "search completion must transition to path reveal");
    updateSearchAnimation(grid.nodes, result, true, animation, 0.025f);
    updateSearchAnimation(grid.nodes, result, true, animation, 0.025f);
    require(playerPath->shape.getFillColor() == sf::Color(255, 245, 120),
            "final route must use the distinct path color");
    require(updateSearchAnimation(
                grid.nodes, result, true, animation, 0.025f),
            "path replay must report completion");

    require(start->shape.getFillColor() == startColor &&
            end->shape.getFillColor() == endColor,
            "animation must preserve start and target appearance");
    require(start->type == START && playerPath->type == PLAYER_PATH &&
            end->type == END,
            "animation must never mutate logical node types");

    restoreGridColors(grid.nodes);
    require(playerPath->shape.getFillColor() == playerPathColor,
            "restoring colors must recover the permanent player-path visual");

    resetAnimationProgress(animation);
    updateSearchAnimation(grid.nodes, result, false, animation, 0.003f);
    require(playerPath->shape.getFillColor() == sf::Color(255, 220, 40),
            "A* discovery must use a distinct frontier color");
    updateSearchAnimation(grid.nodes, result, false, animation, 0.0025f);
    require(playerPath->shape.getFillColor() == sf::Color(235, 130, 35),
            "A* expansion must use a distinct explored color");

    SearchResult noPathResult;
    noPathResult.steps = {{{1, 0}, DISCOVERED}};
    resetAnimationProgress(animation);
    require(updateSearchAnimation(
                grid.nodes, noPathResult, false, animation, 1.f),
            "no-path trace must finish safely without path coordinates");

    for (int i = 0; i < 10; ++i) changeAnimationSpeed(animation, -1);
    require(animationSpeedLabel(animation) == "0.25x",
            "animation speed must clamp at its minimum");
    for (int i = 0; i < 10; ++i) changeAnimationSpeed(animation, 1);
    require(animationSpeedLabel(animation) == "4x",
            "animation speed must clamp at its maximum");
}

void testFinalComparisonOverlayAndPathSegments() {
    TestGrid grid(1, 5);
    grid.nodes[0][0]->setType(START);
    grid.nodes[0][1]->setType(PLAYER_PATH);
    grid.nodes[0][4]->setType(END);

    std::vector<NodeType> originalTypes;
    for (GridNode* node : grid.nodes[0]) originalTypes.push_back(node->type);

    AlgorithmComparison comparison;
    comparison.available = true;
    comparison.status = MATCHING_PATHS;
    comparison.dijkstra.steps = {
        {{1, 0}, DISCOVERED},
        {{1, 0}, EXPANDED},
        {{2, 0}, EXPANDED},
        {{4, 0}, DISCOVERED}
    };
    comparison.astar.steps = {
        {{2, 0}, EXPANDED},
        {{3, 0}, EXPANDED}
    };

    ComparisonOverlay overlay = buildComparisonOverlay(comparison, 1, 5);
    require(overlay[0][0] == NOT_EXPANDED,
            "unexpanded cell must retain its base visual");
    require(overlay[0][1] == DIJKSTRA_ONLY,
            "Dijkstra-only expansion must be classified");
    require(overlay[0][2] == BOTH_EXPANDED,
            "shared expansion must be classified");
    require(overlay[0][3] == ASTAR_ONLY,
            "A*-only expansion must be classified");
    require(overlay[0][4] == NOT_EXPANDED,
            "discovery without expansion must not enter the final overlay");

    sf::Color startColor = grid.nodes[0][0]->shape.getFillColor();
    sf::Color endColor = grid.nodes[0][4]->shape.getFillColor();
    grid.nodes[0][1]->shape.setFillColor(sf::Color::Magenta);
    applyComparisonOverlay(grid.nodes, comparison);
    require(grid.nodes[0][1]->shape.getFillColor() ==
                comparisonCellColor(DIJKSTRA_ONLY),
            "final overlay must replace the terminal animation frame");
    require(grid.nodes[0][2]->shape.getFillColor() ==
                comparisonCellColor(BOTH_EXPANDED),
            "shared cells must use the shared overlay color");
    require(grid.nodes[0][3]->shape.getFillColor() ==
                comparisonCellColor(ASTAR_ONLY),
            "A*-only cells must use the A* overlay color");
    require(grid.nodes[0][0]->shape.getFillColor() == startColor &&
            grid.nodes[0][4]->shape.getFillColor() == endColor,
            "final overlay must preserve start and target visuals");
    for (std::size_t x = 0; x < grid.nodes[0].size(); ++x) {
        require(grid.nodes[0][x]->type == originalTypes[x],
                "final overlay must not mutate logical node state");
    }

    SearchResult dijkstra;
    SearchResult astar;
    dijkstra.path = {{0, 0}, {1, 0}, {2, 0}, {2, 1}};
    astar.path = {{0, 0}, {1, 0}, {1, 1}, {2, 1}};
    std::vector<ComparisonPathSegment> segments =
        buildComparisonPathSegments(dijkstra, astar);

    int sharedCount = 0;
    int dijkstraOnlyCount = 0;
    int astarOnlyCount = 0;
    for (const ComparisonPathSegment& segment : segments) {
        if (segment.state == SHARED_PATH) ++sharedCount;
        if (segment.state == DIJKSTRA_PATH_ONLY) ++dijkstraOnlyCount;
        if (segment.state == ASTAR_PATH_ONLY) ++astarOnlyCount;
    }
    require(sharedCount == 1 && dijkstraOnlyCount == 2 &&
            astarOnlyCount == 2,
            "divergent optimal paths must distinguish shared and unique lines");

    astar.path = dijkstra.path;
    segments = buildComparisonPathSegments(dijkstra, astar);
    require(segments.size() == dijkstra.path.size() - 1,
            "identical paths must render each segment only once");
    for (const ComparisonPathSegment& segment : segments) {
        require(segment.state == SHARED_PATH,
                "identical paths must render as one neutral bright line");
    }

    dijkstra.path = {{40, 0}, {41, 0}};
    astar.path = {{0, 1}, {1, 1}};
    segments = buildComparisonPathSegments(dijkstra, astar);
    require(segments.size() == 2 &&
            segments[0].state == DIJKSTRA_PATH_ONLY &&
            segments[1].state == ASTAR_PATH_ONLY,
            "path segment identity must not collide beyond Classic width");
}

void testComparisonPanelModelAndActions() {
    require(WINDOW_WIDTH == 1320 && BOARD_WIDTH == 1000 &&
            BOARD_HEIGHT == 750,
            "side panel must preserve the exact 1000x750 board geometry");
    require(comparisonPanelActionAt(sf::Vector2f(1030.f, 150.f)) ==
                COMPARE_ACTION,
            "Compare button must expose a clickable panel action");
    require(comparisonPanelActionAt(sf::Vector2f(1120.f, 150.f)) ==
                RESET_ACTION,
            "Reset button must expose a clickable panel action");
    require(comparisonPanelActionAt(sf::Vector2f(1200.f, 150.f)) ==
                CLEAR_ROUTE_ACTION,
            "Clear Route button must expose a clickable panel action");
    require(comparisonPanelActionAt(sf::Vector2f(500.f, 150.f)) ==
                NO_PANEL_ACTION,
            "board clicks must never trigger side-panel controls");

    AlgorithmComparison comparison;
    comparison.available = true;
    comparison.status = MATCHING_PATHS;
    comparison.dijkstra.metrics = {true, 8, 120, 100, 30, 0};
    comparison.astar.metrics = {true, 8, 55, 40, 15, 0};
    AnimationController animation;

    ComparisonPanelText text = buildComparisonPanelText(
        COMPARISON_COMPLETE, 10, comparison, animation
    );
    require(text.stateTitle == "COMPARISON COMPLETE" &&
            text.dijkstraState == "COMPLETE" &&
            text.astarState == "COMPLETE",
            "final panel must expose both algorithm names and states");
    require(text.explorationSummary ==
                "A* expanded 60.0% fewer nodes.",
            "panel efficiency statement must use Dijkstra as its baseline");
    require(text.playerSummary.find("Efficiency: 80.0%") !=
                std::string::npos &&
            text.playerSummary.find("2 steps above optimal") !=
                std::string::npos,
            "panel must compare the player route with the optimal result");

    animation.paused = true;
    text = buildComparisonPanelText(
        ANIMATING_DIJKSTRA, -1, comparison, animation
    );
    require(text.stateTitle == "DIJKSTRA - PAUSED" &&
            text.dijkstraState == "PAUSED" && text.astarState == "WAITING",
            "animation panel must report active and waiting algorithm states");

    animation.paused = false;
    animation.stage = REVEALING_PATH;
    animation.pathStepIndex = 3;
    comparison.dijkstra.path.resize(8);
    text = buildComparisonPanelText(
        ANIMATING_DIJKSTRA, -1, comparison, animation
    );
    require(text.stateTitle == "DIJKSTRA - PATH" &&
            text.dijkstraState == "PATH" &&
            text.stateDetail.find("Path 3/8") != std::string::npos,
            "panel must distinguish path reveal from search replay");

    comparison.dijkstra.metrics.expandedNodes = 20;
    comparison.astar.metrics.expandedNodes = 25;
    text = buildComparisonPanelText(
        COMPARISON_COMPLETE, -1, comparison, animation
    );
    require(text.explorationSummary == "A* expanded 25.0% more nodes.",
            "panel statement must remain correct when A* expands more");
}

void testAllMapIntegration() {
    const std::vector<MapConfig> configs = {
        classicMapConfig(),
        growingTreeMapConfig(21, 31, 101u),
        growingTreeMapConfig(35, 51, 202u),
        growingTreeMapConfig(55, 81, 303u),
        recursiveDivisionMapConfig(21, 31, 404u),
        recursiveDivisionMapConfig(35, 51, 505u),
        recursiveDivisionMapConfig(55, 81, 606u)
    };

    auto finishAnimation = [](Grid& grid,
                              const SearchResult& result,
                              bool dijkstra) {
        AnimationController animation;
        animation.speedIndex = 4;
        bool finished = false;
        for (int update = 0; update < 3 && !finished; ++update) {
            finished = updateSearchAnimation(
                grid, result, dijkstra, animation, 1000.f
            );
        }
        return finished;
    };

    for (const MapConfig& config : configs) {
        LoadedMap map = createMap(config);
        AlgorithmComparison comparison = runAlgorithmComparison(
            map.grid, map.startNode, map.endNode
        );
        require(comparison.available &&
                comparison.status == MATCHING_PATHS &&
                comparison.dijkstra.metrics.found &&
                comparison.astar.metrics.found &&
                comparison.dijkstra.metrics.pathLength ==
                    comparison.astar.metrics.pathLength,
                "every selectable map must produce matching solver results");
        validatePath(
            map.grid, map.startNode, map.endNode, comparison.dijkstra
        );
        validatePath(map.grid, map.startNode, map.endNode, comparison.astar);
        validateMetrics(comparison.dijkstra);
        validateMetrics(comparison.astar);

        for (const GridPosition& position : comparison.dijkstra.path) {
            GridNode* node = map.grid[position.y][position.x];
            if (node != map.startNode && node != map.endNode) {
                node->setType(PLAYER_PATH);
            }
        }
        require(computeHandDrawnPathLength(
                    map.grid, map.startNode, map.endNode
                ) == comparison.dijkstra.metrics.pathLength,
                "player route detection must work on every selectable map");

        std::vector<NodeType> permanentTypes = captureNodeTypes(map.grid);
        restoreGridColors(map.grid);
        require(finishAnimation(map.grid, comparison.dijkstra, true),
                "Dijkstra animation must finish on every selectable map");
        restoreGridColors(map.grid);
        require(finishAnimation(map.grid, comparison.astar, false),
                "A* animation must finish on every selectable map");

        ComparisonOverlay overlay = buildComparisonOverlay(
            comparison, map.grid.size(), map.grid.front().size()
        );
        require(overlay.size() == map.grid.size(),
                "comparison overlay must retain the active row count");
        for (const auto& row : overlay) {
            require(row.size() == map.grid.front().size(),
                    "comparison overlay must retain the active column count");
        }
        applyComparisonOverlay(map.grid, comparison);
        require(captureNodeTypes(map.grid) == permanentTypes,
                "comparison overlay must not alter player or map state");

        std::vector<ComparisonPathSegment> segments =
            buildComparisonPathSegments(
                comparison.dijkstra, comparison.astar
            );
        require(!segments.empty(),
                "every selectable map must expose a final path overlay");
        for (const ComparisonPathSegment& segment : segments) {
            for (const GridPosition& position :
                    {segment.start, segment.end}) {
                require(position.y >= 0 &&
                        position.y < static_cast<int>(map.grid.size()) &&
                        position.x >= 0 &&
                        position.x < static_cast<int>(
                            map.grid[position.y].size()
                        ),
                        "final path overlay must stay within map dimensions");
            }
        }

        AlgorithmComparison repeated = runAlgorithmComparison(
            map.grid, map.startNode, map.endNode, false
        );
        require(repeated.status == MATCHING_PATHS &&
                repeated.dijkstra.metrics.pathLength ==
                    comparison.dijkstra.metrics.pathLength &&
                repeated.astar.metrics.pathLength ==
                    comparison.astar.metrics.pathLength,
                "repeated comparison must remain stable on every map");

        bool largestPreset = config.rows == 55 && config.cols == 81;
        std::size_t warmupRuns = largestPreset ? 10 : 1;
        std::size_t measuredRuns = largestPreset ? 200 : 7;
        BenchmarkMetrics benchmark = benchmarkAlgorithms(
            map.grid, map.startNode, map.endNode,
            warmupRuns, measuredRuns
        );
        require(benchmark.available &&
                benchmark.warmupRuns == warmupRuns &&
                benchmark.measuredRuns == measuredRuns &&
                benchmark.dijkstraMedianNanoseconds >= 0 &&
                benchmark.astarMedianNanoseconds >= 0,
                "benchmark must complete and report metrics on every map");
    }
}

void testRepeatedBenchmark() {
    TestGrid grid(6, 6);
    GridNode* start = grid.nodes[0][0];
    GridNode* end = grid.nodes[5][5];
    start->setType(START);
    end->setType(END);
    grid.nodes[2][2]->setType(OBSTACLE);
    grid.nodes[3][2]->setType(PLAYER_PATH);

    std::vector<NodeType> originalTypes;
    for (const auto& row : grid.nodes) {
        for (GridNode* node : row) originalTypes.push_back(node->type);
    }

    BenchmarkMetrics benchmark = benchmarkAlgorithms(
        grid.nodes, start, end, 2, 9
    );
    require(benchmark.available && benchmark.warmupRuns == 2 &&
            benchmark.measuredRuns == 9,
            "benchmark must report its warm-up and measured run counts");
    require(benchmark.dijkstraMedianNanoseconds >= 0 &&
            benchmark.astarMedianNanoseconds >= 0,
            "benchmark medians must be valid non-negative durations");

    std::size_t typeIndex = 0;
    for (const auto& row : grid.nodes) {
        for (GridNode* node : row) {
            require(node->type == originalTypes[typeIndex++],
                    "benchmark runs must preserve permanent grid state");
        }
    }

    BenchmarkMetrics emptyBenchmark = benchmarkAlgorithms(
        grid.nodes, start, end, 2, 0
    );
    require(!emptyBenchmark.available && emptyBenchmark.measuredRuns == 0,
            "zero measured runs must not report a benchmark result");
}

int main() {
    try {
        testClassicMapModelAndReplacement();
        testMapSelectionModelAndActions();
        testGrowingTreeMazeGeneration();
        testRecursiveDivisionMazeGeneration();
        testStandardMapOptimalityAndMetrics();
        testDynamicGridLifetimeEditingAndHandRoute();
        testKnownPathLengthAndPlayerPathTraversability();
        testRepeatedDeterministicRuns();
        testNoPathAndStaleParentReset();
        testAnimationReplayAndLogicalStateSafety();
        testFinalComparisonOverlayAndPathSegments();
        testComparisonPanelModelAndActions();
        testAllMapIntegration();
        testRepeatedBenchmark();
    } catch (const std::exception& error) {
        std::cerr << "PathfindingGameTests failed: " << error.what() << '\n';
        return 1;
    }

    std::cout << "PathfindingGameTests passed\n";
    return 0;
}
