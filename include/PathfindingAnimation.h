#ifndef PATHFINDINGANIMATION_H
#define PATHFINDINGANIMATION_H

#include "PathfindingGame.h"

#include <cstddef>
#include <string>
#include <vector>

enum GameState {
    EDITING,
    PLAYER_ROUTE_COMPLETE,
    ANIMATING_DIJKSTRA,
    HOLDING_DIJKSTRA,
    ANIMATING_ASTAR,
    HOLDING_ASTAR,
    COMPARISON_COMPLETE,
    NO_PATH_RESULT,
    COMPARISON_ERROR
};

enum AnimationStage { REPLAYING_SEARCH, REVEALING_PATH };

struct AnimationController {
    std::size_t searchStepIndex = 0;
    std::size_t pathStepIndex = 0;
    AnimationStage stage = REPLAYING_SEARCH;
    float stepAccumulator = 0.f;
    float holdElapsed = 0.f;
    bool paused = false;
    int speedIndex = 2;
    sf::Clock frameClock;
};

bool isAnimationState(GameState state);
void resetAnimationProgress(AnimationController& animation);
void restoreGridColors(std::vector<std::vector<GridNode*>>& grid);
void changeAnimationSpeed(AnimationController& animation, int direction);
const std::string& animationSpeedLabel(const AnimationController& animation);

bool updateSearchAnimation(std::vector<std::vector<GridNode*>>& grid,
                           const SearchResult& result,
                           bool dijkstra,
                           AnimationController& animation,
                           float elapsedSeconds);

#endif
