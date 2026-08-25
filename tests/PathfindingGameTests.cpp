#include "PathfindingGame.h"
#include "PathfindingAnimation.h"

#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class TestGrid {
public:
    std::vector<std::vector<GridNode*>> nodes;

    TestGrid(int rows, int cols)
        : nodes(rows, std::vector<GridNode*>(cols)) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                nodes[y][x] = new GridNode(x, y);
            }
        }
    }

    ~TestGrid() {
        for (auto& row : nodes) {
            for (GridNode* node : row) {
                delete node;
            }
        }
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

void validatePath(const TestGrid& grid,
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
                position.y < static_cast<int>(grid.nodes.size()),
                "path row is outside the grid");
        require(position.x >= 0 &&
                position.x < static_cast<int>(grid.nodes[position.y].size()),
                "path column is outside the grid");
        require(grid.nodes[position.y][position.x]->type != OBSTACLE,
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

void addStandardObstacles(TestGrid& grid,
                          GridNode* start,
                          GridNode* end) {
    auto markColumnWithGap = [&](int col, int gapRow) {
        for (int y = 0; y < static_cast<int>(grid.nodes.size()); ++y) {
            GridNode* node = grid.nodes[y][col];
            if (node == start || node == end || y == gapRow) continue;
            node->setType(OBSTACLE);
        }
    };

    markColumnWithGap(COL_COUNT / 4, ROW_COUNT / 3);
    markColumnWithGap(COL_COUNT / 2, ROW_COUNT / 2);
    markColumnWithGap(3 * COL_COUNT / 4, 2 * ROW_COUNT / 3);
}

void testStandardMapOptimalityAndMetrics() {
    TestGrid grid(ROW_COUNT, COL_COUNT);
    GridNode* start = grid.nodes[5][5];
    GridNode* end = grid.nodes[15][25];
    start->setType(START);
    end->setType(END);
    addStandardObstacles(grid, start, end);
    grid.nodes[5][6]->setType(PLAYER_PATH);

    std::vector<NodeType> originalTypes;
    for (const auto& row : grid.nodes) {
        for (GridNode* node : row) {
            originalTypes.push_back(node->type);
        }
    }

    AlgorithmComparison comparison = beginAlgorithmComparison(
        grid.nodes, start, end
    );
    require(!comparison.available,
            "comparison must wait for A* before reporting completion");
    require(comparison.dijkstra.metrics.found && comparison.astar.path.empty(),
            "staged comparison must retain Dijkstra before running A*");
    completeAlgorithmComparison(comparison, grid.nodes, start, end);
    const SearchResult& dijkstraResult = comparison.dijkstra;
    const SearchResult& astarResult = comparison.astar;

    require(comparison.available, "comparison must retain both algorithm results");
    require(comparison.status == MATCHING_PATHS,
            "standard-map comparison must report matching optimal paths");
    validatePath(grid, start, end, dijkstraResult);
    validatePath(grid, start, end, astarResult);
    validateMetrics(dijkstraResult);
    validateMetrics(astarResult);
    validateTrace(dijkstraResult, start, end, COL_COUNT);
    validateTrace(astarResult, start, end, COL_COUNT);
    require(dijkstraResult.metrics.pathLength == astarResult.metrics.pathLength,
            "Dijkstra and A* must find the same optimal length");
    require(start->hCost == 30.0,
            "A* must initialize the start Manhattan heuristic");

    std::size_t typeIndex = 0;
    for (const auto& row : grid.nodes) {
        for (GridNode* node : row) {
            require(node->type == originalTypes[typeIndex++],
                    "comparison must not mutate logical board state");
        }
    }
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
    validatePath(grid, start, end, dijkstraResult);
    validatePath(grid, start, end, astarResult);
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

int main() {
    try {
        testStandardMapOptimalityAndMetrics();
        testKnownPathLengthAndPlayerPathTraversability();
        testRepeatedDeterministicRuns();
        testNoPathAndStaleParentReset();
        testAnimationReplayAndLogicalStateSafety();
    } catch (const std::exception& error) {
        std::cerr << "PathfindingGameTests failed: " << error.what() << '\n';
        return 1;
    }

    std::cout << "PathfindingGameTests passed\n";
    return 0;
}
