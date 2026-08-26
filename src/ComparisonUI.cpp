#include "ComparisonUI.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {

const float PANEL_X = static_cast<float>(BOARD_WIDTH);
const sf::Color PANEL_BACKGROUND(13, 18, 30);
const sf::Color CARD_BACKGROUND(23, 30, 47);
const sf::Color CARD_OUTLINE(49, 61, 82);
const sf::Color PRIMARY_TEXT(238, 242, 250);
const sf::Color SECONDARY_TEXT(151, 163, 184);
const sf::Color DIJKSTRA_ACCENT(45, 190, 235);
const sf::Color ASTAR_ACCENT(245, 157, 52);
const sf::Color SUCCESS_ACCENT(50, 205, 135);
const sf::Color BUTTON_BACKGROUND(39, 50, 70);
const sf::Color BUTTON_HOVER(56, 72, 99);
const sf::Color BUTTON_DISABLED(28, 35, 49);

const sf::FloatRect COMPARE_BUTTON(
    sf::Vector2f(PANEL_X + 18.f, 137.f), sf::Vector2f(84.f, 34.f)
);
const sf::FloatRect RESET_BUTTON(
    sf::Vector2f(PANEL_X + 108.f, 137.f), sf::Vector2f(72.f, 34.f)
);
const sf::FloatRect CLEAR_BUTTON(
    sf::Vector2f(PANEL_X + 186.f, 137.f), sf::Vector2f(116.f, 34.f)
);

void drawText(sf::RenderTarget& target,
              const sf::Font& font,
              const std::string& value,
              unsigned int size,
              sf::Vector2f position,
              sf::Color color = PRIMARY_TEXT) {
    sf::Text text(font, value, size);
    text.setPosition(position);
    text.setFillColor(color);
    target.draw(text);
}

void drawCard(sf::RenderTarget& target,
              sf::Vector2f position,
              sf::Vector2f size) {
    sf::RectangleShape card(size);
    card.setPosition(position);
    card.setFillColor(CARD_BACKGROUND);
    card.setOutlineColor(CARD_OUTLINE);
    card.setOutlineThickness(1.f);
    target.draw(card);
}

void drawButton(sf::RenderTarget& target,
                const sf::Font& font,
                const sf::FloatRect& bounds,
                const std::string& label,
                bool enabled,
                bool hovered) {
    sf::RectangleShape button(bounds.size);
    button.setPosition(bounds.position);
    button.setFillColor(!enabled
        ? BUTTON_DISABLED
        : (hovered ? BUTTON_HOVER : BUTTON_BACKGROUND));
    button.setOutlineColor(enabled ? CARD_OUTLINE : BUTTON_DISABLED);
    button.setOutlineThickness(1.f);
    target.draw(button);

    sf::Text text(font, label, 12);
    sf::FloatRect textBounds = text.getLocalBounds();
    text.setPosition(sf::Vector2f(
        bounds.position.x + (bounds.size.x - textBounds.size.x) / 2.f -
            textBounds.position.x,
        bounds.position.y + 8.f
    ));
    text.setFillColor(enabled ? PRIMARY_TEXT : SECONDARY_TEXT);
    target.draw(text);
}

bool hasDijkstraResult(GameState state) {
    return state == ANIMATING_DIJKSTRA || state == HOLDING_DIJKSTRA ||
           state == ANIMATING_ASTAR || state == HOLDING_ASTAR ||
           state == COMPARISON_COMPLETE || state == NO_PATH_RESULT ||
           state == COMPARISON_ERROR;
}

std::string pathMetric(const SearchResult& result, bool available) {
    if (!available) return "--";
    return result.metrics.found
        ? std::to_string(result.metrics.pathLength)
        : "NO PATH";
}

std::string countMetric(std::size_t value, bool available) {
    return available ? std::to_string(value) : "--";
}

std::string benchmarkTime(long long nanoseconds, bool available) {
    if (!available) return "--";
    double microseconds = static_cast<double>(nanoseconds) / 1000.0;
    std::ostringstream text;
    text << std::fixed << std::setprecision(microseconds < 100.0 ? 1 : 0)
         << microseconds << " us";
    return text.str();
}

std::string percentage(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(1) << value << '%';
    return text.str();
}

std::string mapSummary(const MapConfig& config) {
    std::string summary;
    switch (config.type) {
        case CLASSIC_MAP:
            summary = "CLASSIC";
            break;
        case GROWING_TREE_MAZE:
            summary = "GROWING TREE";
            break;
        case RECURSIVE_DIVISION_MAZE:
            summary = "RECURSIVE DIVISION";
            break;
    }

    summary += " | " + std::to_string(config.cols) + " x " +
        std::to_string(config.rows);
    if (config.type != CLASSIC_MAP) {
        summary += " | SEED " + std::to_string(config.seed);
    }
    return summary;
}

void drawMetricRow(sf::RenderTarget& target,
                   const sf::Font& font,
                   const std::string& label,
                   const std::string& dijkstraValue,
                   const std::string& astarValue,
                   float y) {
    drawText(target, font, label, 11, sf::Vector2f(PANEL_X + 18.f, y),
             SECONDARY_TEXT);
    drawText(target, font, dijkstraValue, 12,
             sf::Vector2f(PANEL_X + 126.f, y - 1.f));
    drawText(target, font, astarValue, 12,
             sf::Vector2f(PANEL_X + 228.f, y - 1.f));
}

void drawExpandedBar(sf::RenderTarget& target,
                     const sf::Font& font,
                     const std::string& label,
                     std::size_t value,
                     std::size_t maximum,
                     float y,
                     sf::Color color) {
    drawText(target, font, label, 11, sf::Vector2f(PANEL_X + 18.f, y), color);

    sf::RectangleShape track(sf::Vector2f(186.f, 8.f));
    track.setPosition(sf::Vector2f(PANEL_X + 59.f, y + 3.f));
    track.setFillColor(sf::Color(34, 43, 60));
    target.draw(track);

    float ratio = maximum == 0
        ? 0.f
        : static_cast<float>(value) / static_cast<float>(maximum);
    sf::RectangleShape bar(sf::Vector2f(186.f * ratio, 8.f));
    bar.setPosition(track.getPosition());
    bar.setFillColor(color);
    target.draw(bar);

    drawText(target, font, std::to_string(value), 11,
             sf::Vector2f(PANEL_X + 255.f, y), PRIMARY_TEXT);
}

void drawLegend(sf::RenderTarget& target, const sf::Font& font) {
    drawText(target, font, "FINAL OVERLAY", 11,
             sf::Vector2f(PANEL_X + 18.f, 559.f), SECONDARY_TEXT);

    const std::string labels[] = {
        "Dijkstra only", "A* only", "Both explored", "Optimal path"
    };
    const sf::Color colors[] = {
        comparisonCellColor(DIJKSTRA_ONLY),
        comparisonCellColor(ASTAR_ONLY),
        comparisonCellColor(BOTH_EXPANDED),
        comparisonPathColor(SHARED_PATH)
    };

    for (int i = 0; i < 4; ++i) {
        float y = 579.f + static_cast<float>(i * 18);
        sf::RectangleShape swatch(
            i == 3 ? sf::Vector2f(17.f, 4.f) : sf::Vector2f(14.f, 12.f)
        );
        swatch.setPosition(sf::Vector2f(
            PANEL_X + 18.f, y + (i == 3 ? 4.f : 0.f)
        ));
        swatch.setFillColor(colors[i]);
        target.draw(swatch);
        drawText(target, font, labels[i], 11,
                 sf::Vector2f(PANEL_X + 43.f, y - 2.f));
    }
}

} // namespace

ComparisonPanelAction comparisonPanelActionAt(sf::Vector2f position) {
    if (COMPARE_BUTTON.contains(position)) return COMPARE_ACTION;
    if (RESET_BUTTON.contains(position)) return RESET_ACTION;
    if (CLEAR_BUTTON.contains(position)) return CLEAR_ROUTE_ACTION;
    return NO_PANEL_ACTION;
}

ComparisonPanelText buildComparisonPanelText(
    GameState state,
    int playerPathLength,
    const AlgorithmComparison& comparison,
    const AnimationController& animation
) {
    ComparisonPanelText text;

    switch (state) {
        case EDITING:
            text.stateTitle = "READY TO EXPLORE";
            text.stateDetail = "Draw a route or compare immediately.";
            break;
        case PLAYER_ROUTE_COMPLETE:
            text.stateTitle = "PLAYER ROUTE COMPLETE";
            text.stateDetail = "Ready to compare against the optimum.";
            break;
        case ANIMATING_DIJKSTRA:
        case HOLDING_DIJKSTRA:
            {
            bool revealingPath = animation.stage == REVEALING_PATH;
            text.stateTitle = animation.paused
                ? "DIJKSTRA - PAUSED"
                : (state == HOLDING_DIJKSTRA
                    ? "DIJKSTRA - COMPLETE"
                    : (revealingPath
                        ? "DIJKSTRA - PATH"
                        : "DIJKSTRA - SEARCHING"));
            text.stateDetail = "Speed " + animationSpeedLabel(animation) +
                (revealingPath
                    ? " | Path " + std::to_string(animation.pathStepIndex) +
                        "/" + std::to_string(comparison.dijkstra.path.size())
                    : " | Search " +
                        std::to_string(animation.searchStepIndex) + "/" +
                        std::to_string(comparison.dijkstra.steps.size()));
            break;
            }
        case ANIMATING_ASTAR:
        case HOLDING_ASTAR:
            {
            bool revealingPath = animation.stage == REVEALING_PATH;
            text.stateTitle = animation.paused
                ? "A* - PAUSED"
                : (state == HOLDING_ASTAR
                    ? "A* - COMPLETE"
                    : (revealingPath ? "A* - PATH" : "A* - SEARCHING"));
            text.stateDetail = "Speed " + animationSpeedLabel(animation) +
                (revealingPath
                    ? " | Path " + std::to_string(animation.pathStepIndex) +
                        "/" + std::to_string(comparison.astar.path.size())
                    : " | Search " +
                        std::to_string(animation.searchStepIndex) + "/" +
                        std::to_string(comparison.astar.steps.size()));
            break;
            }
        case COMPARISON_COMPLETE:
            text.stateTitle = "COMPARISON COMPLETE";
            text.stateDetail = "Optimal routes found on the same board.";
            break;
        case NO_PATH_RESULT:
            text.stateTitle = "NO PATH";
            text.stateDetail = "Neither algorithm can reach the target.";
            break;
        case COMPARISON_ERROR:
            text.stateTitle = "RESULT MISMATCH";
            text.stateDetail = "The solvers did not agree on the result.";
            break;
    }

    if (state == EDITING || state == PLAYER_ROUTE_COMPLETE) {
        text.dijkstraState = "READY";
        text.astarState = "READY";
    } else if (state == ANIMATING_DIJKSTRA) {
        text.dijkstraState = animation.paused
            ? "PAUSED"
            : (animation.stage == REVEALING_PATH ? "PATH" : "SEARCHING");
        text.astarState = "WAITING";
    } else if (state == HOLDING_DIJKSTRA) {
        text.dijkstraState = "COMPLETE";
        text.astarState = "WAITING";
    } else if (state == ANIMATING_ASTAR) {
        text.dijkstraState = "COMPLETE";
        text.astarState = animation.paused
            ? "PAUSED"
            : (animation.stage == REVEALING_PATH ? "PATH" : "SEARCHING");
    } else {
        text.dijkstraState = "COMPLETE";
        text.astarState = state == COMPARISON_ERROR ? "ERROR" : "COMPLETE";
    }

    if (!comparison.available) {
        text.explorationSummary =
            "Run both algorithms to compare search effort.";
    } else {
        std::size_t dijkstraExpanded = comparison.dijkstra.metrics.expandedNodes;
        std::size_t astarExpanded = comparison.astar.metrics.expandedNodes;
        if (dijkstraExpanded == astarExpanded) {
            text.explorationSummary = "Both expanded the same number of nodes.";
        } else if (dijkstraExpanded == 0) {
            text.explorationSummary = "A* expanded more nodes on this board.";
        } else {
            double difference = 100.0 *
                (static_cast<double>(dijkstraExpanded) -
                 static_cast<double>(astarExpanded)) /
                static_cast<double>(dijkstraExpanded);
            if (difference > 0.0) {
                text.explorationSummary = "A* expanded " +
                    percentage(difference) + " fewer nodes.";
            } else {
                text.explorationSummary = "A* expanded " +
                    percentage(std::abs(difference)) + " more nodes.";
            }
        }
    }

    if (playerPathLength < 0) {
        text.playerSummary =
            "No completed route.\nDraw from start to target.";
    } else if (!comparison.available ||
               !comparison.dijkstra.metrics.found) {
        text.playerSummary = "Route: " + std::to_string(playerPathLength) +
            " steps\nCompare to score efficiency.";
    } else {
        int optimalLength = comparison.dijkstra.metrics.pathLength;
        double efficiency = playerPathLength == 0
            ? 100.0
            : 100.0 * static_cast<double>(optimalLength) /
                static_cast<double>(playerPathLength);
        efficiency = std::clamp(efficiency, 0.0, 100.0);
        int extraSteps = std::max(0, playerPathLength - optimalLength);

        text.playerSummary = "Route " + std::to_string(playerPathLength) +
            " | Optimal " + std::to_string(optimalLength) +
            "\nEfficiency: " + percentage(efficiency) + "\n" +
            (extraSteps == 0
                ? "Matches the optimal route"
                : std::to_string(extraSteps) + " steps above optimal");
    }

    return text;
}

void drawComparisonPanel(sf::RenderTarget& target,
                         const sf::Font& font,
                         const MapConfig& mapConfig,
                         GameState state,
                         int playerPathLength,
                         const AlgorithmComparison& comparison,
                         const AnimationController& animation,
                         sf::Vector2f mousePosition) {
    sf::RectangleShape panel(
        sf::Vector2f(static_cast<float>(SIDE_PANEL_WIDTH),
                     static_cast<float>(BOARD_HEIGHT))
    );
    panel.setPosition(sf::Vector2f(PANEL_X, 0.f));
    panel.setFillColor(PANEL_BACKGROUND);
    target.draw(panel);

    ComparisonPanelText panelText = buildComparisonPanelText(
        state, playerPathLength, comparison, animation
    );

    drawText(target, font, "PATHFINDING LAB", 22,
             sf::Vector2f(PANEL_X + 18.f, 14.f));
    drawText(target, font, mapSummary(mapConfig), 9,
             sf::Vector2f(PANEL_X + 19.f, 43.f), SECONDARY_TEXT);

    drawCard(target, sf::Vector2f(PANEL_X + 18.f, 70.f),
             sf::Vector2f(284.f, 55.f));
    sf::Color stateAccent = SUCCESS_ACCENT;
    if (state == ANIMATING_DIJKSTRA || state == HOLDING_DIJKSTRA) {
        stateAccent = DIJKSTRA_ACCENT;
    } else if (state == ANIMATING_ASTAR || state == HOLDING_ASTAR) {
        stateAccent = ASTAR_ACCENT;
    } else if (state == NO_PATH_RESULT || state == COMPARISON_ERROR) {
        stateAccent = sf::Color(238, 82, 104);
    } else if (state == EDITING) {
        stateAccent = SECONDARY_TEXT;
    }
    drawText(target, font, panelText.stateTitle, 13,
             sf::Vector2f(PANEL_X + 30.f, 79.f), stateAccent);
    drawText(target, font, panelText.stateDetail, 11,
             sf::Vector2f(PANEL_X + 30.f, 101.f), SECONDARY_TEXT);

    bool compareEnabled = !isAnimationState(state);
    ComparisonPanelAction hovered = comparisonPanelActionAt(mousePosition);
    drawButton(target, font, COMPARE_BUTTON, "Compare", compareEnabled,
               hovered == COMPARE_ACTION);
    drawButton(target, font, RESET_BUTTON, "Reset", true,
               hovered == RESET_ACTION);
    drawButton(target, font, CLEAR_BUTTON, "Clear Route", true,
               hovered == CLEAR_ROUTE_ACTION);

    drawText(target, font, "ALGORITHM COMPARISON", 11,
             sf::Vector2f(PANEL_X + 18.f, 183.f), SECONDARY_TEXT);
    drawText(target, font, "DIJKSTRA", 12,
             sf::Vector2f(PANEL_X + 126.f, 202.f), DIJKSTRA_ACCENT);
    drawText(target, font, "A*", 12,
             sf::Vector2f(PANEL_X + 228.f, 202.f), ASTAR_ACCENT);
    drawText(target, font, panelText.dijkstraState, 9,
             sf::Vector2f(PANEL_X + 126.f, 219.f), SECONDARY_TEXT);
    drawText(target, font, panelText.astarState, 9,
             sf::Vector2f(PANEL_X + 228.f, 219.f), SECONDARY_TEXT);

    bool dijkstraAvailable = hasDijkstraResult(state);
    bool astarAvailable = comparison.available;
    drawMetricRow(target, font, "Path length",
                  pathMetric(comparison.dijkstra, dijkstraAvailable),
                  pathMetric(comparison.astar, astarAvailable), 241.f);
    drawMetricRow(target, font, "Expanded",
                  countMetric(comparison.dijkstra.metrics.expandedNodes,
                              dijkstraAvailable),
                  countMetric(comparison.astar.metrics.expandedNodes,
                              astarAvailable), 262.f);
    drawMetricRow(target, font, "Discovered",
                  countMetric(comparison.dijkstra.metrics.discoveredNodes,
                              dijkstraAvailable),
                  countMetric(comparison.astar.metrics.discoveredNodes,
                              astarAvailable), 283.f);
    drawMetricRow(target, font, "Peak frontier",
                  countMetric(comparison.dijkstra.metrics.maxFrontierSize,
                              dijkstraAvailable),
                  countMetric(comparison.astar.metrics.maxFrontierSize,
                              astarAvailable), 304.f);

    std::string benchmarkLabel = "MEDIAN SEARCH TIME";
    if (comparison.benchmark.available) {
        benchmarkLabel += " (" +
            std::to_string(comparison.benchmark.measuredRuns) + " RUNS)";
    }
    drawText(target, font, benchmarkLabel, 10,
             sf::Vector2f(PANEL_X + 18.f, 329.f), SECONDARY_TEXT);
    drawText(target, font,
             benchmarkTime(
                 comparison.benchmark.dijkstraMedianNanoseconds,
                 comparison.benchmark.available
             ),
             11, sf::Vector2f(PANEL_X + 126.f, 346.f), DIJKSTRA_ACCENT);
    drawText(target, font,
             benchmarkTime(
                 comparison.benchmark.astarMedianNanoseconds,
                 comparison.benchmark.available
             ),
             11, sf::Vector2f(PANEL_X + 228.f, 346.f), ASTAR_ACCENT);

    drawText(target, font, "NODES EXPANDED", 11,
             sf::Vector2f(PANEL_X + 18.f, 370.f), SECONDARY_TEXT);
    std::size_t dijkstraExpanded = dijkstraAvailable
        ? comparison.dijkstra.metrics.expandedNodes : 0;
    std::size_t astarExpanded = astarAvailable
        ? comparison.astar.metrics.expandedNodes : 0;
    std::size_t maximumExpanded = std::max(dijkstraExpanded, astarExpanded);
    drawExpandedBar(target, font, "D", dijkstraExpanded, maximumExpanded,
                    391.f, DIJKSTRA_ACCENT);
    drawExpandedBar(target, font, "A*", astarExpanded, maximumExpanded,
                    414.f, ASTAR_ACCENT);
    drawText(target, font, panelText.explorationSummary, 11,
             sf::Vector2f(PANEL_X + 18.f, 440.f), PRIMARY_TEXT);

    drawCard(target, sf::Vector2f(PANEL_X + 18.f, 466.f),
             sf::Vector2f(284.f, 81.f));
    drawText(target, font, "PLAYER", 11,
             sf::Vector2f(PANEL_X + 30.f, 475.f), SECONDARY_TEXT);
    drawText(target, font, panelText.playerSummary, 11,
             sf::Vector2f(PANEL_X + 30.f, 494.f), PRIMARY_TEXT);

    drawLegend(target, font);

    drawText(target, font,
             "Dijkstra: cost-so-far\n"
             "A*: cost + Manhattan estimate\n"
             "Wheel zoom | Middle drag pan | F fit\n"
             "Left/right drag: draw/erase\n"
             "Space compare | P pause | -/+ speed\n"
             "R reset | C clear | Esc cancel\n"
             "N new map | M map selection",
             10, sf::Vector2f(PANEL_X + 18.f, 649.f), SECONDARY_TEXT);
}
