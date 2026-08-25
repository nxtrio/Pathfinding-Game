#include "PathfindingAnimation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <utility>

const std::array<float, 5> ANIMATION_SPEEDS = {0.25f, 0.5f, 1.f, 2.f, 4.f};
const std::array<std::string, 5> ANIMATION_SPEED_LABELS = {
    "0.25x", "0.5x", "1x", "2x", "4x"
};

const sf::Color DIJKSTRA_FRONTIER_COLOR(0, 220, 255);
const sf::Color DIJKSTRA_EXPANDED_COLOR(40, 120, 210);
const sf::Color ASTAR_FRONTIER_COLOR(255, 220, 40);
const sf::Color ASTAR_EXPANDED_COLOR(235, 130, 35);
const sf::Color FINAL_PATH_COLOR(255, 245, 120);
const sf::Color DIJKSTRA_ONLY_COLOR(35, 145, 215);
const sf::Color ASTAR_ONLY_COLOR(235, 125, 35);
const sf::Color BOTH_EXPANDED_COLOR(115, 85, 175);
const sf::Color DIJKSTRA_PATH_COLOR(80, 230, 255);
const sf::Color ASTAR_PATH_COLOR(255, 180, 60);
const sf::Color SHARED_PATH_COLOR(255, 245, 170);

bool isAnimationState(GameState state) {
    return state == ANIMATING_DIJKSTRA || state == HOLDING_DIJKSTRA ||
           state == ANIMATING_ASTAR || state == HOLDING_ASTAR;
}

void resetAnimationProgress(AnimationController& animation) {
    animation.searchStepIndex = 0;
    animation.pathStepIndex = 0;
    animation.stage = REPLAYING_SEARCH;
    animation.stepAccumulator = 0.f;
    animation.holdElapsed = 0.f;
    animation.frameClock.restart();
}

void restoreGridColors(std::vector<std::vector<GridNode*>>& grid) {
    for (auto& row : grid) {
        for (GridNode* node : row) {
            node->setType(node->type);
        }
    }
}

void changeAnimationSpeed(AnimationController& animation, int direction) {
    animation.speedIndex = std::clamp(
        animation.speedIndex + direction,
        0,
        static_cast<int>(ANIMATION_SPEEDS.size()) - 1
    );
}

const std::string& animationSpeedLabel(const AnimationController& animation) {
    return ANIMATION_SPEED_LABELS[animation.speedIndex];
}

static void applySearchStep(std::vector<std::vector<GridNode*>>& grid,
                            const SearchStep& step,
                            bool dijkstra) {
    GridNode* node = grid[step.position.y][step.position.x];
    if (node->type == START || node->type == END || node->type == OBSTACLE) return;

    if (step.type == DISCOVERED) {
        node->shape.setFillColor(
            dijkstra ? DIJKSTRA_FRONTIER_COLOR : ASTAR_FRONTIER_COLOR
        );
    } else {
        node->shape.setFillColor(
            dijkstra ? DIJKSTRA_EXPANDED_COLOR : ASTAR_EXPANDED_COLOR
        );
    }
}

static void applyPathStep(std::vector<std::vector<GridNode*>>& grid,
                          const GridPosition& position) {
    GridNode* node = grid[position.y][position.x];
    if (node->type == START || node->type == END || node->type == OBSTACLE) return;
    node->shape.setFillColor(FINAL_PATH_COLOR);
}

static float animationStepInterval(const AnimationController& animation) {
    float speed = ANIMATION_SPEEDS[animation.speedIndex];
    return animation.stage == REPLAYING_SEARCH
        ? 0.0025f / speed
        : 0.025f / speed;
}

bool updateSearchAnimation(std::vector<std::vector<GridNode*>>& grid,
                           const SearchResult& result,
                           bool dijkstra,
                           AnimationController& animation,
                           float elapsedSeconds) {
    animation.stepAccumulator += elapsedSeconds;

    while (true) {
        if (animation.stage == REPLAYING_SEARCH &&
            animation.searchStepIndex >= result.steps.size()) {
            animation.stage = REVEALING_PATH;
            animation.stepAccumulator = 0.f;
        }

        if (animation.stage == REVEALING_PATH &&
            animation.pathStepIndex >= result.path.size()) {
            return true;
        }

        float interval = animationStepInterval(animation);
        if (animation.stepAccumulator < interval) return false;
        animation.stepAccumulator -= interval;

        if (animation.stage == REPLAYING_SEARCH) {
            applySearchStep(
                grid, result.steps[animation.searchStepIndex++], dijkstra
            );
        } else {
            applyPathStep(grid, result.path[animation.pathStepIndex++]);
        }
    }
}

ComparisonOverlay buildComparisonOverlay(const AlgorithmComparison& comparison,
                                         std::size_t rowCount,
                                         std::size_t columnCount) {
    ComparisonOverlay overlay(
        rowCount,
        std::vector<ComparisonCellState>(columnCount, NOT_EXPANDED)
    );
    std::vector<bool> dijkstraExpanded(rowCount * columnCount, false);
    std::vector<bool> astarExpanded(rowCount * columnCount, false);

    auto recordExpanded = [&](const SearchResult& result,
                              std::vector<bool>& membership) {
        for (const SearchStep& step : result.steps) {
            if (step.type != EXPANDED || step.position.x < 0 ||
                step.position.y < 0 ||
                static_cast<std::size_t>(step.position.x) >= columnCount ||
                static_cast<std::size_t>(step.position.y) >= rowCount) {
                continue;
            }

            std::size_t key = static_cast<std::size_t>(step.position.y) *
                              columnCount +
                              static_cast<std::size_t>(step.position.x);
            membership[key] = true;
        }
    };

    recordExpanded(comparison.dijkstra, dijkstraExpanded);
    recordExpanded(comparison.astar, astarExpanded);

    for (std::size_t y = 0; y < rowCount; ++y) {
        for (std::size_t x = 0; x < columnCount; ++x) {
            std::size_t key = y * columnCount + x;
            if (dijkstraExpanded[key] && astarExpanded[key]) {
                overlay[y][x] = BOTH_EXPANDED;
            } else if (dijkstraExpanded[key]) {
                overlay[y][x] = DIJKSTRA_ONLY;
            } else if (astarExpanded[key]) {
                overlay[y][x] = ASTAR_ONLY;
            }
        }
    }

    return overlay;
}

sf::Color comparisonCellColor(ComparisonCellState state) {
    switch (state) {
        case DIJKSTRA_ONLY:
            return DIJKSTRA_ONLY_COLOR;
        case ASTAR_ONLY:
            return ASTAR_ONLY_COLOR;
        case BOTH_EXPANDED:
            return BOTH_EXPANDED_COLOR;
        case NOT_EXPANDED:
            return sf::Color::Transparent;
    }
    return sf::Color::Transparent;
}

void applyComparisonOverlay(std::vector<std::vector<GridNode*>>& grid,
                            const AlgorithmComparison& comparison) {
    restoreGridColors(grid);
    if (grid.empty() || grid.front().empty()) return;

    ComparisonOverlay overlay = buildComparisonOverlay(
        comparison, grid.size(), grid.front().size()
    );
    for (std::size_t y = 0; y < grid.size(); ++y) {
        for (std::size_t x = 0; x < grid[y].size(); ++x) {
            GridNode* node = grid[y][x];
            ComparisonCellState state = overlay[y][x];
            if (state != NOT_EXPANDED && node->type != START &&
                node->type != END && node->type != OBSTACLE) {
                node->shape.setFillColor(comparisonCellColor(state));
            }
        }
    }
}

static std::pair<int, int> pathSegmentKey(const GridPosition& start,
                                          const GridPosition& end) {
    int startKey = start.y * COL_COUNT + start.x;
    int endKey = end.y * COL_COUNT + end.x;
    return std::minmax(startKey, endKey);
}

static std::set<std::pair<int, int>> collectPathSegmentKeys(
    const SearchResult& result
) {
    std::set<std::pair<int, int>> keys;
    for (std::size_t i = 1; i < result.path.size(); ++i) {
        keys.insert(pathSegmentKey(result.path[i - 1], result.path[i]));
    }
    return keys;
}

std::vector<ComparisonPathSegment> buildComparisonPathSegments(
    const SearchResult& dijkstra,
    const SearchResult& astar
) {
    std::vector<ComparisonPathSegment> segments;
    std::set<std::pair<int, int>> dijkstraKeys =
        collectPathSegmentKeys(dijkstra);
    std::set<std::pair<int, int>> astarKeys = collectPathSegmentKeys(astar);

    for (std::size_t i = 1; i < dijkstra.path.size(); ++i) {
        std::pair<int, int> key = pathSegmentKey(
            dijkstra.path[i - 1], dijkstra.path[i]
        );
        segments.push_back({
            dijkstra.path[i - 1],
            dijkstra.path[i],
            astarKeys.count(key) == 1 ? SHARED_PATH : DIJKSTRA_PATH_ONLY
        });
    }

    for (std::size_t i = 1; i < astar.path.size(); ++i) {
        std::pair<int, int> key = pathSegmentKey(
            astar.path[i - 1], astar.path[i]
        );
        if (dijkstraKeys.count(key) == 0) {
            segments.push_back({
                astar.path[i - 1], astar.path[i], ASTAR_PATH_ONLY
            });
        }
    }

    return segments;
}

sf::Color comparisonPathColor(ComparisonPathState state) {
    switch (state) {
        case DIJKSTRA_PATH_ONLY:
            return DIJKSTRA_PATH_COLOR;
        case ASTAR_PATH_ONLY:
            return ASTAR_PATH_COLOR;
        case SHARED_PATH:
            return SHARED_PATH_COLOR;
    }
    return SHARED_PATH_COLOR;
}

void drawComparisonPaths(sf::RenderTarget& target,
                         const AlgorithmComparison& comparison) {
    std::vector<ComparisonPathSegment> segments = buildComparisonPathSegments(
        comparison.dijkstra, comparison.astar
    );

    for (const ComparisonPathSegment& segment : segments) {
        const float centerOffset = static_cast<float>(CELL_SIZE - 1) / 2.f;
        sf::Vector2f start(
            static_cast<float>(segment.start.x * CELL_SIZE) + centerOffset,
            static_cast<float>(segment.start.y * CELL_SIZE) + centerOffset
        );
        sf::Vector2f end(
            static_cast<float>(segment.end.x * CELL_SIZE) + centerOffset,
            static_cast<float>(segment.end.y * CELL_SIZE) + centerOffset
        );
        sf::Vector2f delta = end - start;
        float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        float thickness = segment.state == SHARED_PATH ? 5.f : 3.f;

        sf::RectangleShape line(sf::Vector2f(length, thickness));
        line.setOrigin(sf::Vector2f(0.f, thickness / 2.f));
        line.setPosition(start);
        line.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
        line.setFillColor(comparisonPathColor(segment.state));
        target.draw(line);
    }
}
