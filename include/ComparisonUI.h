#ifndef COMPARISONUI_H
#define COMPARISONUI_H

#include "PathfindingAnimation.h"

#include <string>

const int BOARD_WIDTH = COL_COUNT * CELL_SIZE;
const int BOARD_HEIGHT = ROW_COUNT * CELL_SIZE;
const int SIDE_PANEL_WIDTH = 320;
const int WINDOW_WIDTH = BOARD_WIDTH + SIDE_PANEL_WIDTH;

enum ComparisonPanelAction {
    NO_PANEL_ACTION,
    COMPARE_ACTION,
    RESET_ACTION,
    CLEAR_ROUTE_ACTION
};

struct ComparisonPanelText {
    std::string stateTitle;
    std::string stateDetail;
    std::string dijkstraState;
    std::string astarState;
    std::string explorationSummary;
    std::string playerSummary;
};

ComparisonPanelAction comparisonPanelActionAt(sf::Vector2f position);

ComparisonPanelText buildComparisonPanelText(
    GameState state,
    int playerPathLength,
    const AlgorithmComparison& comparison,
    const AnimationController& animation
);

void drawComparisonPanel(sf::RenderTarget& target,
                         const sf::Font& font,
                         GameState state,
                         int playerPathLength,
                         const AlgorithmComparison& comparison,
                         const AnimationController& animation,
                         sf::Vector2f mousePosition);

#endif
