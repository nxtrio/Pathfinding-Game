#include "PathfindingGame.h"
#include "PathfindingAnimation.h"
#include "ComparisonUI.h"
#include <iostream>
#include <optional>

namespace {

constexpr int CLASSIC_ROWS = 30;
constexpr int CLASSIC_COLS = 40;

// Generate some non-trivial permanent obstacles
void generateObstacles(Grid& grid,
                       GridNode* startNode,
                       GridNode* endNode)
{
    if (grid.empty() || grid.front().empty()) return;
    const int rows = static_cast<int>(grid.size());
    const int cols = static_cast<int>(grid.front().size());

    auto markColumnWithGap = [&](int col, int gapRow) {
        if (col < 0 || col >= cols) return;
        for (int y = 0; y < rows; ++y) {
            if (col >= static_cast<int>(grid[y].size())) continue;
            GridNode* n = grid[y][col];
            if (n == startNode || n == endNode) continue;
            if (y == gapRow) continue; // leave a gap to ensure a possible route
            n->setType(OBSTACLE);
        }
    };

    // Three vertical obstacle “walls” with staggered gaps so path has to snake around
    markColumnWithGap(cols / 4, rows / 3);
    markColumnWithGap(cols / 2, rows / 2);
    markColumnWithGap(3 * cols / 4, 2 * rows / 3);
}

} // namespace

int main() {
    // SFML 3: VideoMode takes a Vector2u
    sf::RenderWindow window(
        sf::VideoMode({static_cast<unsigned int>(WINDOW_WIDTH),
                       static_cast<unsigned int>(BOARD_HEIGHT)}),
        "Pathfinding Challenge - Player vs Dijkstra vs A*"
    );
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::Font font;
    if (!font.openFromFile("assets/arial.ttf")) {
        std::cerr << "Failed to load font for HUD (assets/arial.ttf)\n";
        return 1;
    }

    // Initialize Grid
    Grid grid = createGrid(CLASSIC_ROWS, CLASSIC_COLS);

    // Set Default Start and End
    GridNode* startNode = grid[5][5];
    GridNode* endNode   = grid[15][25];
    startNode->setType(START);
    endNode->setType(END);

    // Generate permanent obstacles (non-editable)
    generateObstacles(grid, startNode, endNode);

    GameState gameState = EDITING;
    AlgorithmComparison comparisonResults;
    AnimationController animation;
    int playerPathLength = -1;
    int previousMouseX = -1;
    int previousMouseY = -1;
    NodeType previousEditType = EMPTY;

    while (window.isOpen()) {
        bool compareRequested = false;
        bool cancelRequested = false;
        bool resetRequested = false;
        bool clearRequested = false;

        // --- Events ---
        while (const std::optional eventOpt = window.pollEvent()) {
            const sf::Event& event = *eventOpt;

            // Window closed
            if (event.is<sf::Event::Closed>()) {
                window.close();
                continue;
            }

            if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::Space &&
                    !isAnimationState(gameState)) {
                    compareRequested = true;
                } else if (keyPressed->code == sf::Keyboard::Key::P &&
                           isAnimationState(gameState)) {
                    animation.paused = !animation.paused;
                } else if ((keyPressed->code == sf::Keyboard::Key::Hyphen ||
                            keyPressed->code == sf::Keyboard::Key::Subtract) &&
                           isAnimationState(gameState)) {
                    changeAnimationSpeed(animation, -1);
                } else if ((keyPressed->code == sf::Keyboard::Key::Equal ||
                            keyPressed->code == sf::Keyboard::Key::Add) &&
                           isAnimationState(gameState)) {
                    changeAnimationSpeed(animation, 1);
                } else if (keyPressed->code == sf::Keyboard::Key::Escape &&
                           isAnimationState(gameState)) {
                    cancelRequested = true;
                } else if (keyPressed->code == sf::Keyboard::Key::R) {
                    resetRequested = true;
                } else if (keyPressed->code == sf::Keyboard::Key::C) {
                    clearRequested = true;
                }
            }

            if (const auto* mousePressed =
                    event.getIf<sf::Event::MouseButtonPressed>()) {
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    ComparisonPanelAction action = comparisonPanelActionAt(
                        sf::Vector2f(mousePressed->position)
                    );
                    if (action == COMPARE_ACTION &&
                        !isAnimationState(gameState)) {
                        compareRequested = true;
                    } else if (action == RESET_ACTION) {
                        resetRequested = true;
                    } else if (action == CLEAR_ROUTE_ACTION) {
                        clearRequested = true;
                    }
                }
            }
        }

        if (!window.isOpen()) break;

        float animationElapsed = animation.frameClock.restart().asSeconds();

        if (clearRequested) {
            restoreGridColors(grid);
            for (auto& row : grid) {
                for (GridNode* node : row) {
                    if (node->type == PLAYER_PATH) node->setType(EMPTY);
                }
            }
            comparisonResults = AlgorithmComparison{};
            playerPathLength = -1;
            animation.paused = false;
            resetAnimationProgress(animation);
            animationElapsed = 0.f;
            compareRequested = false;
            gameState = EDITING;
        } else if (cancelRequested || resetRequested) {
            restoreGridColors(grid);
            comparisonResults = AlgorithmComparison{};
            animation.paused = false;
            resetAnimationProgress(animation);
            animationElapsed = 0.f;
            gameState = playerPathLength >= 0 ? PLAYER_ROUTE_COMPLETE : EDITING;
        }

        if (compareRequested) {
            restoreGridColors(grid);
            comparisonResults = beginAlgorithmComparison(
                grid, startNode, endNode
            );
            animation.paused = false;
            resetAnimationProgress(animation);
            animationElapsed = 0.f;
            gameState = ANIMATING_DIJKSTRA;
        }

        // --- Continuous mouse input ---
        bool leftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        bool rightPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
        bool editingEnabled = gameState == EDITING ||
                              gameState == PLAYER_ROUTE_COMPLETE;

        if (editingEnabled && (leftPressed || rightPressed)) {
            sf::Vector2i pos = sf::Mouse::getPosition(window);
            int x = pos.x / CELL_SIZE;
            int y = pos.y / CELL_SIZE;
            NodeType editType = rightPressed ? EMPTY : PLAYER_PATH;

            bool insideGrid = y >= 0 &&
                              y < static_cast<int>(grid.size()) &&
                              x >= 0 &&
                              x < static_cast<int>(grid[y].size());
            if (pos.x >= 0 && pos.x < BOARD_WIDTH &&
                pos.y >= 0 && pos.y < BOARD_HEIGHT && insideGrid) {
                int lineStartX = x;
                int lineStartY = y;
                if (previousMouseX >= 0 && previousMouseY >= 0 &&
                    previousEditType == editType) {
                    lineStartX = previousMouseX;
                    lineStartY = previousMouseY;
                }

                editGridLine(grid, startNode, endNode,
                             lineStartX, lineStartY, x, y, editType);
                previousMouseX = x;
                previousMouseY = y;
                previousEditType = editType;
            } else {
                previousMouseX = -1;
                previousMouseY = -1;
            }
        } else {
            previousMouseX = -1;
            previousMouseY = -1;
        }

        // --- Hand-drawn path detection ---
        if (editingEnabled) {
            playerPathLength = computeHandDrawnPathLength(
                grid, startNode, endNode
            );
            gameState = playerPathLength >= 0 ? PLAYER_ROUTE_COMPLETE : EDITING;
        }

        // --- Non-blocking search animation ---
        if (isAnimationState(gameState) && !animation.paused) {
            if (gameState == ANIMATING_DIJKSTRA) {
                if (updateSearchAnimation(
                        grid, comparisonResults.dijkstra, true,
                        animation, animationElapsed)) {
                    gameState = HOLDING_DIJKSTRA;
                    animation.holdElapsed = 0.f;
                }
            } else if (gameState == HOLDING_DIJKSTRA) {
                animation.holdElapsed += animationElapsed;
                if (animation.holdElapsed >= 0.75f) {
                    restoreGridColors(grid);
                    completeAlgorithmComparison(
                        comparisonResults, grid, startNode, endNode
                    );
                    if (comparisonResults.status == MISMATCHED_RESULTS) {
                        gameState = COMPARISON_ERROR;
                    } else {
                        resetAnimationProgress(animation);
                        gameState = ANIMATING_ASTAR;
                    }
                }
            } else if (gameState == ANIMATING_ASTAR) {
                if (updateSearchAnimation(
                        grid, comparisonResults.astar, false,
                        animation, animationElapsed)) {
                    gameState = HOLDING_ASTAR;
                    animation.holdElapsed = 0.f;
                }
            } else if (gameState == HOLDING_ASTAR) {
                animation.holdElapsed += animationElapsed;
                if (animation.holdElapsed >= 0.75f) {
                    comparisonResults.benchmark = benchmarkAlgorithms(
                        grid, startNode, endNode
                    );
                    applyComparisonOverlay(grid, comparisonResults);
                    gameState = comparisonResults.status == BOTH_NO_PATH
                        ? NO_PATH_RESULT
                        : COMPARISON_COMPLETE;
                }
            }
        }

        // --- Render ---
        window.clear(sf::Color(9, 14, 25));
        for (const auto& row : grid) {
            for (GridNode* node : row) {
                window.draw(node->shape);
            }
        }
        if (gameState == COMPARISON_COMPLETE ||
            gameState == NO_PATH_RESULT) {
            drawComparisonPaths(window, comparisonResults);
        }
        sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
        drawComparisonPanel(
            window, font, gameState, playerPathLength, comparisonResults,
            animation, sf::Vector2f(mousePosition)
        );
        window.display();

    }

    // Cleanup
    destroyGrid(grid);

    return 0;
}
