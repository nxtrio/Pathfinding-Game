#include "PathfindingAnimation.h"

#include <algorithm>
#include <array>

const std::array<float, 5> ANIMATION_SPEEDS = {0.25f, 0.5f, 1.f, 2.f, 4.f};
const std::array<std::string, 5> ANIMATION_SPEED_LABELS = {
    "0.25x", "0.5x", "1x", "2x", "4x"
};

const sf::Color DIJKSTRA_FRONTIER_COLOR(0, 220, 255);
const sf::Color DIJKSTRA_EXPANDED_COLOR(40, 120, 210);
const sf::Color ASTAR_FRONTIER_COLOR(255, 220, 40);
const sf::Color ASTAR_EXPANDED_COLOR(235, 130, 35);
const sf::Color FINAL_PATH_COLOR(255, 245, 120);

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
