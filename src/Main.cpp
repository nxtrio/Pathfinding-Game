#include "PathfindingGame.h"
#include "PathfindingAnimation.h"
#include <cmath>
#include <optional>
#include <string>
#include <vector>
#include <iostream>
#include <queue>
#include <sstream>

std::string formatPathLength(const SearchResult& result) {
    if (!result.metrics.found) return "NO PATH";
    return std::to_string(result.metrics.pathLength);
}

std::string formatAlgorithmMetrics(const std::string& name,
                                   const SearchResult& result) {
    std::ostringstream text;
    text << name << '\n'
         << "  Path length: " << formatPathLength(result) << '\n'
         << "  Discovered: " << result.metrics.discoveredNodes << '\n'
         << "  Expanded: " << result.metrics.expandedNodes << '\n'
         << "  Peak frontier: " << result.metrics.maxFrontierSize << '\n'
         << "  Single run: " << result.metrics.singleRunMicroseconds << " us\n";
    return text.str();
}

std::string formatHudText(GameState state,
                          int playerPathLength,
                          const AlgorithmComparison& results,
                          const AnimationController& animation) {
    if (isAnimationState(state)) {
        bool dijkstra = state == ANIMATING_DIJKSTRA ||
                        state == HOLDING_DIJKSTRA;
        bool holding = state == HOLDING_DIJKSTRA || state == HOLDING_ASTAR;
        const SearchResult& activeResult = dijkstra
            ? results.dijkstra
            : results.astar;

        std::ostringstream text;
        text << (dijkstra ? "DIJKSTRA" : "A*")
             << (holding
                    ? (activeResult.metrics.found
                        ? " - PATH COMPLETE\n"
                        : " - SEARCH COMPLETE\n")
                    : " - SEARCHING\n");
        if (animation.paused) text << "PAUSED\n";
        text << "Speed: " << animationSpeedLabel(animation) << '\n'
             << "Search: " << animation.searchStepIndex << " / "
             << activeResult.steps.size() << '\n'
             << "Path: " << animation.pathStepIndex << " / "
             << activeResult.path.size() << "\n\n"
             << formatAlgorithmMetrics(
                    dijkstra ? "DIJKSTRA" : "A*", activeResult
                )
             << "\nP: Pause  -/+: Speed  Esc/R: Cancel";
        return text.str();
    }

    if (results.available &&
        (state == COMPARISON_COMPLETE || state == NO_PATH_RESULT ||
         state == COMPARISON_ERROR)) {
        std::ostringstream text;
        if (state == COMPARISON_COMPLETE) {
            text << "ALGORITHM COMPARISON\n";
        } else if (state == NO_PATH_RESULT) {
            text << "ALGORITHM COMPARISON - NO PATH\n";
        } else {
            text << "ALGORITHM COMPARISON - RESULT MISMATCH\n";
        }

        if (playerPathLength >= 0) {
            text << "Player route: " << playerPathLength << " steps\n";
        }
        text << '\n'
             << formatAlgorithmMetrics("DIJKSTRA", results.dijkstra) << '\n'
             << formatAlgorithmMetrics("A*", results.astar) << '\n'
             << "Space: Compare again  R: Reset";
        return text.str();
    }

    if (state == PLAYER_ROUTE_COMPLETE) {
        return "PLAYER ROUTE COMPLETE\nLength: " +
               std::to_string(playerPathLength) +
               " steps\n\nSpace: Compare algorithms";
    }

    return "Path length: N/A\nSpace: Compare algorithms";
}

void drawComparisonLegend(sf::RenderTarget& target, const sf::Font& font) {
    const sf::Vector2f origin(
        static_cast<float>(COL_COUNT * CELL_SIZE - 245),
        static_cast<float>(ROW_COUNT * CELL_SIZE - 120)
    );
    sf::RectangleShape background(sf::Vector2f(235.f, 110.f));
    background.setPosition(origin);
    background.setFillColor(sf::Color(15, 18, 28, 220));
    background.setOutlineColor(sf::Color(220, 225, 240, 180));
    background.setOutlineThickness(1.f);
    target.draw(background);

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
        float y = origin.y + 11.f + static_cast<float>(i * 24);
        sf::RectangleShape swatch(
            i == 3 ? sf::Vector2f(18.f, 5.f) : sf::Vector2f(18.f, 14.f)
        );
        swatch.setPosition(sf::Vector2f(
            origin.x + 12.f, y + (i == 3 ? 5.f : 0.f)
        ));
        swatch.setFillColor(colors[i]);
        target.draw(swatch);

        sf::Text label(font, labels[i], 14);
        label.setPosition(sf::Vector2f(origin.x + 40.f, y - 3.f));
        label.setFillColor(sf::Color::White);
        target.draw(label);
    }
}

// Generate some non-trivial permanent obstacles
void generateObstacles(std::vector<std::vector<GridNode*>>& grid,
                       GridNode* startNode,
                       GridNode* endNode)
{
    auto markColumnWithGap = [&](int col, int gapRow) {
        if (col < 0 || col >= COL_COUNT) return;
        for (int y = 0; y < ROW_COUNT; ++y) {
            GridNode* n = grid[y][col];
            if (n == startNode || n == endNode) continue;
            if (y == gapRow) continue; // leave a gap to ensure a possible route
            n->setType(OBSTACLE);
        }
    };

    // Three vertical obstacle “walls” with staggered gaps so path has to snake around
    markColumnWithGap(COL_COUNT / 4, ROW_COUNT / 3);
    markColumnWithGap(COL_COUNT / 2, ROW_COUNT / 2);
    markColumnWithGap(3 * COL_COUNT / 4, 2 * ROW_COUNT / 3);
}

// BFS that walks ONLY along hand-drawn path cells (START/PLAYER_PATH/END).
// Returns number of steps from start to end, or -1 if not connected.
int computeHandDrawnPathLength(const std::vector<std::vector<GridNode*>>& grid,
                               GridNode* startNode,
                               GridNode* endNode)
{
    const int rows = ROW_COUNT;
    const int cols = COL_COUNT;

    std::vector<std::vector<int>> dist(rows, std::vector<int>(cols, -1));
    std::queue<std::pair<int, int>> q;

    int sx = startNode->x;
    int sy = startNode->y;
    int ex = endNode->x;
    int ey = endNode->y;

    dist[sy][sx] = 0;
    q.push({sy, sx});

    auto canWalk = [&](GridNode* n) -> bool {
        return (n->type == START || n->type == PLAYER_PATH || n->type == END);
    };

    while (!q.empty()) {
        auto [y, x] = q.front();
        q.pop();

        if (x == ex && y == ey) {
            return dist[y][x];  // number of steps along hand-drawn path
        }

        const int dx[4] = { 1, -1, 0, 0 };
        const int dy[4] = { 0, 0, 1, -1 };

        for (int dir = 0; dir < 4; ++dir) {
            int nx = x + dx[dir];
            int ny = y + dy[dir];

            if (nx < 0 || nx >= cols || ny < 0 || ny >= rows) continue;
            if (dist[ny][nx] != -1) continue;

            GridNode* neighbor = grid[ny][nx];
            if (!canWalk(neighbor)) continue;  // skip EMPTY / OBSTACLE / other types

            dist[ny][nx] = dist[y][x] + 1;
            q.push({ny, nx});
        }
    }

    return -1; // not connected
}

// Draw or erase every cell between two sampled mouse positions.
void editGridLine(std::vector<std::vector<GridNode*>>& grid,
                  GridNode* startNode,
                  GridNode* endNode,
                  int startX,
                  int startY,
                  int endX,
                  int endY,
                  NodeType editType)
{
    int x = startX;
    int y = startY;
    int dx = std::abs(endX - startX);
    int dy = std::abs(endY - startY);
    int stepX = startX < endX ? 1 : -1;
    int stepY = startY < endY ? 1 : -1;
    int error = dx - dy;

    while (true) {
        if (x >= 0 && x < COL_COUNT && y >= 0 && y < ROW_COUNT) {
            GridNode* n = grid[y][x];
            bool canDraw = editType == PLAYER_PATH &&
                           (n->type == EMPTY || n->type == PLAYER_PATH);
            bool canErase = editType == EMPTY && n->type == PLAYER_PATH;
            if (n != startNode && n != endNode && n->type != OBSTACLE &&
                (canDraw || canErase)) {
                n->setType(editType);
            }
        }

        if (x == endX && y == endY) break;

        int doubledError = 2 * error;
        if (doubledError > -dy) {
            error -= dy;
            x += stepX;
        } else if (doubledError < dx) {
            error += dx;
            y += stepY;
        }
    }
}

int main() {
    // SFML 3: VideoMode takes a Vector2u
    sf::RenderWindow window(
        sf::VideoMode({static_cast<unsigned int>(COL_COUNT * CELL_SIZE),
                       static_cast<unsigned int>(ROW_COUNT * CELL_SIZE)}),
        "Pathfinding Game"
    );
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::Font font;
    if (!font.openFromFile("assets/arial.ttf")) {
        std::cerr << "Failed to load font for HUD (assets/arial.ttf)\n";
        return 1;
    }

    // Initialize Grid
    std::vector<std::vector<GridNode*>> grid(
        ROW_COUNT, std::vector<GridNode*>(COL_COUNT)
    );
    for (int y = 0; y < ROW_COUNT; ++y) {
        for (int x = 0; x < COL_COUNT; ++x) {
            grid[y][x] = new GridNode(x, y);
        }
    }

    // Set Default Start and End
    GridNode* startNode = grid[5][5];
    GridNode* endNode   = grid[15][25];
    startNode->setType(START);
    endNode->setType(END);

    // Generate permanent obstacles (non-editable)
    generateObstacles(grid, startNode, endNode);

    // --- HUD: path length text ---
    sf::Text pathText(font, "", 18);
    pathText.setFillColor(sf::Color::Black);
    pathText.setOutlineColor(sf::Color::White);
    pathText.setOutlineThickness(1.f);
    pathText.setPosition(sf::Vector2f(10.f, 10.f));
    pathText.setString("Path length: N/A\nSpace: Compare algorithms");

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
                }
            }
        }

        if (!window.isOpen()) break;

        float animationElapsed = animation.frameClock.restart().asSeconds();

        if (cancelRequested || resetRequested) {
            restoreGridColors(grid);
            comparisonResults = AlgorithmComparison{};
            animation.paused = false;
            resetAnimationProgress(animation);
            animationElapsed = 0.f;
            gameState = playerPathLength >= 0 ? PLAYER_ROUTE_COMPLETE : EDITING;
            pathText.setString(formatHudText(
                gameState, playerPathLength, comparisonResults, animation
            ));
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
            pathText.setString(formatHudText(
                gameState, playerPathLength, comparisonResults, animation
            ));
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

            if (pos.x >= 0 && pos.y >= 0 &&
                x < COL_COUNT && y < ROW_COUNT) {
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
            pathText.setString(formatHudText(
                gameState, playerPathLength, comparisonResults, animation
            ));
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
                    applyComparisonOverlay(grid, comparisonResults);
                    gameState = comparisonResults.status == BOTH_NO_PATH
                        ? NO_PATH_RESULT
                        : COMPARISON_COMPLETE;
                }
            }
        }

        if (isAnimationState(gameState)) {
            pathText.setString(formatHudText(
                gameState, playerPathLength, comparisonResults, animation
            ));
        } else if (gameState == COMPARISON_COMPLETE ||
                   gameState == NO_PATH_RESULT ||
                   gameState == COMPARISON_ERROR) {
            pathText.setString(formatHudText(
                gameState, playerPathLength, comparisonResults, animation
            ));
        }

        // --- Render ---
        window.clear();
        for (int y = 0; y < ROW_COUNT; ++y) {
            for (int x = 0; x < COL_COUNT; ++x) {
                window.draw(grid[y][x]->shape);
            }
        }
        if (gameState == COMPARISON_COMPLETE ||
            gameState == NO_PATH_RESULT) {
            drawComparisonPaths(window, comparisonResults);
            drawComparisonLegend(window, font);
        }
        window.draw(pathText);
        window.display();

    }

    // Cleanup
    for (auto& row : grid) {
        for (auto& node : row) {
            delete node;
        }
    }

    return 0;
}
