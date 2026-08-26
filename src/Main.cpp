#include "PathfindingGame.h"
#include "PathfindingAnimation.h"
#include "ComparisonUI.h"
#include "MapGeneration.h"
#include "MapSelectionUI.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <random>
#include <utility>

namespace {

constexpr float MAP_FIT_MARGIN = 1.05f;
constexpr float MIN_CAMERA_SCALE = 0.35f;
constexpr float MAX_CAMERA_SCALE = 3.f;
constexpr float WHEEL_ZOOM_BASE = 0.85f;

enum AppScreen { MAP_SELECTION, GAME };

struct CameraState {
    sf::View worldView;
    sf::Vector2f fittedViewSize{
        static_cast<float>(BOARD_WIDTH),
        static_cast<float>(BOARD_HEIGHT)
    };
    float scale = 1.f;
};

sf::Vector2f mapWorldSize(const Grid& grid) {
    std::size_t columnCount = 0;
    for (const auto& row : grid) {
        columnCount = std::max(columnCount, row.size());
    }

    return sf::Vector2f(
        static_cast<float>(columnCount * CELL_SIZE),
        static_cast<float>(grid.size() * CELL_SIZE)
    );
}

void clampCameraToMap(CameraState& camera, const Grid& grid) {
    sf::Vector2f worldSize = mapWorldSize(grid);
    if (worldSize.x <= 0.f || worldSize.y <= 0.f) return;

    sf::Vector2f viewSize = camera.worldView.getSize();
    sf::Vector2f center = camera.worldView.getCenter();

    auto clampAxis = [](float currentCenter,
                        float currentViewSize,
                        float currentWorldSize) {
        if (currentViewSize >= currentWorldSize) {
            return currentWorldSize / 2.f;
        }
        float halfView = currentViewSize / 2.f;
        return std::clamp(
            currentCenter, halfView, currentWorldSize - halfView
        );
    };

    center.x = clampAxis(center.x, viewSize.x, worldSize.x);
    center.y = clampAxis(center.y, viewSize.y, worldSize.y);
    camera.worldView.setCenter(center);
}

void fitCameraToMap(CameraState& camera, const Grid& grid) {
    sf::Vector2f worldSize = mapWorldSize(grid);
    if (worldSize.x <= 0.f || worldSize.y <= 0.f) return;

    constexpr float boardAspect =
        static_cast<float>(BOARD_WIDTH) / static_cast<float>(BOARD_HEIGHT);
    float fittedWidth = worldSize.x * MAP_FIT_MARGIN;
    float fittedHeight = worldSize.y * MAP_FIT_MARGIN;

    if (fittedWidth / fittedHeight > boardAspect) {
        fittedHeight = fittedWidth / boardAspect;
    } else {
        fittedWidth = fittedHeight * boardAspect;
    }

    // Keep small maps at no more than roughly one screen pixel per world unit.
    if (fittedWidth < static_cast<float>(BOARD_WIDTH)) {
        fittedWidth = static_cast<float>(BOARD_WIDTH);
        fittedHeight = static_cast<float>(BOARD_HEIGHT);
    }

    camera.fittedViewSize = sf::Vector2f(fittedWidth, fittedHeight);
    camera.scale = 1.f;
    camera.worldView.setSize(camera.fittedViewSize);
    camera.worldView.setCenter(worldSize / 2.f);
    clampCameraToMap(camera, grid);
}

bool pixelIsInsideBoard(const sf::RenderWindow& window,
                        const CameraState& camera,
                        sf::Vector2i pixel) {
    return window.getViewport(camera.worldView).contains(pixel);
}

std::optional<GridPosition> gridPositionAtPixel(
    const sf::RenderWindow& window,
    const CameraState& camera,
    const Grid& grid,
    sf::Vector2i pixel
) {
    if (!pixelIsInsideBoard(window, camera, pixel)) return std::nullopt;
    return worldToGridPosition(
        grid, window.mapPixelToCoords(pixel, camera.worldView)
    );
}

void zoomCameraAtPixel(CameraState& camera,
                       const Grid& grid,
                       const sf::RenderWindow& window,
                       sf::Vector2i pixel,
                       float wheelDelta) {
    if (!pixelIsInsideBoard(window, camera, pixel)) return;

    sf::Vector2f worldBefore = window.mapPixelToCoords(
        pixel, camera.worldView
    );
    float zoomMultiplier = static_cast<float>(
        std::pow(WHEEL_ZOOM_BASE, wheelDelta)
    );
    camera.scale = std::clamp(
        camera.scale * zoomMultiplier,
        MIN_CAMERA_SCALE,
        MAX_CAMERA_SCALE
    );
    camera.worldView.setSize(sf::Vector2f(
        camera.fittedViewSize.x * camera.scale,
        camera.fittedViewSize.y * camera.scale
    ));

    sf::Vector2f worldAfter = window.mapPixelToCoords(
        pixel, camera.worldView
    );
    camera.worldView.move(worldBefore - worldAfter);
    clampCameraToMap(camera, grid);
}

void loadMap(LoadedMap& activeMap,
             const MapConfig& config,
             CameraState& camera,
             GameState& gameState,
             AlgorithmComparison& comparison,
             AnimationController& animation,
             int& playerPathLength,
             int& previousMouseX,
             int& previousMouseY,
             NodeType& previousEditType,
             bool& middleDragging) {
    // Construct first so a failed map request leaves the active map untouched.
    LoadedMap replacement = createMap(config);
    activeMap = std::move(replacement);

    comparison = AlgorithmComparison{};
    playerPathLength = -1;
    previousMouseX = -1;
    previousMouseY = -1;
    previousEditType = EMPTY;
    middleDragging = false;
    animation.paused = false;
    resetAnimationProgress(animation);
    gameState = EDITING;
    fitCameraToMap(camera, activeMap.grid);
}

void unloadMap(LoadedMap& activeMap,
               GameState& gameState,
               AlgorithmComparison& comparison,
               AnimationController& animation,
               int& playerPathLength,
               int& previousMouseX,
               int& previousMouseY,
               NodeType& previousEditType,
               bool& middleDragging) {
    activeMap = LoadedMap{};
    comparison = AlgorithmComparison{};
    playerPathLength = -1;
    previousMouseX = -1;
    previousMouseY = -1;
    previousEditType = EMPTY;
    middleDragging = false;
    animation.paused = false;
    resetAnimationProgress(animation);
    gameState = EDITING;
}

unsigned int randomSeedDifferentFrom(unsigned int currentSeed) {
    static std::mt19937 seedGenerator(std::random_device{}());
    unsigned int nextSeed = seedGenerator();
    if (nextSeed == currentSeed) {
        nextSeed = seedGenerator();
        if (nextSeed == currentSeed) ++nextSeed;
    }
    return nextSeed;
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

    LoadedMap activeMap;
    Grid& grid = activeMap.grid;
    GridNode*& startNode = activeMap.startNode;
    GridNode*& endNode = activeMap.endNode;

    sf::View uiView = window.getDefaultView();
    CameraState camera;
    const sf::FloatRect boardViewport(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(
            static_cast<float>(BOARD_WIDTH) /
                static_cast<float>(WINDOW_WIDTH),
            1.f
        )
    );
    camera.worldView.setViewport(boardViewport);
    camera.worldView.setScissor(boardViewport);

    GameState gameState = EDITING;
    AlgorithmComparison comparisonResults;
    AnimationController animation;
    int playerPathLength = -1;
    int previousMouseX = -1;
    int previousMouseY = -1;
    NodeType previousEditType = EMPTY;
    bool middleDragging = false;
    sf::Vector2i previousPanPixel;
    AppScreen appScreen = MAP_SELECTION;
    MapSelectionState mapSelection;

    while (window.isOpen()) {
        bool compareRequested = false;
        bool cancelRequested = false;
        bool resetRequested = false;
        bool clearRequested = false;
        bool startMapRequested = false;
        bool returnToSelectionRequested = false;
        bool regenerateRequested = false;

        // --- Events ---
        while (const std::optional eventOpt = window.pollEvent()) {
            const sf::Event& event = *eventOpt;

            // Window closed
            if (event.is<sf::Event::Closed>()) {
                window.close();
                continue;
            }

            if (event.is<sf::Event::FocusLost>()) {
                middleDragging = false;
                previousMouseX = -1;
                previousMouseY = -1;
            }

            if (appScreen == MAP_SELECTION) {
                if (const auto* keyPressed =
                        event.getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->code == sf::Keyboard::Key::Escape) {
                        window.close();
                    }
                }

                if (const auto* mousePressed =
                        event.getIf<sf::Event::MouseButtonPressed>()) {
                    if (mousePressed->button == sf::Mouse::Button::Left) {
                        sf::Vector2f uiPosition = window.mapPixelToCoords(
                            mousePressed->position, uiView
                        );
                        switch (mapSelectionActionAt(uiPosition)) {
                            case SELECT_CLASSIC_ACTION:
                                mapSelection.selectedType = CLASSIC_MAP;
                                break;
                            case SELECT_GROWING_TREE_ACTION:
                                mapSelection.selectedType = GROWING_TREE_MAZE;
                                break;
                            case SELECT_RECURSIVE_DIVISION_ACTION:
                                mapSelection.selectedType =
                                    RECURSIVE_DIVISION_MAZE;
                                break;
                            case SELECT_SMALL_SIZE_ACTION:
                                mapSelection.selectedSize = SMALL_MAP_SIZE;
                                break;
                            case SELECT_MEDIUM_SIZE_ACTION:
                                mapSelection.selectedSize = MEDIUM_MAP_SIZE;
                                break;
                            case SELECT_LARGE_SIZE_ACTION:
                                mapSelection.selectedSize = LARGE_MAP_SIZE;
                                break;
                            case RANDOMIZE_SEED_ACTION:
                                mapSelection.seed = randomSeedDifferentFrom(
                                    mapSelection.seed
                                );
                                break;
                            case START_MAP_ACTION:
                                startMapRequested = true;
                                break;
                            case NO_MAP_SELECTION_ACTION:
                                break;
                        }
                    }
                }
                continue;
            }

            if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::M) {
                    returnToSelectionRequested = true;
                } else if (keyPressed->code == sf::Keyboard::Key::N) {
                    regenerateRequested = true;
                } else if (keyPressed->code == sf::Keyboard::Key::F) {
                    fitCameraToMap(camera, grid);
                    previousMouseX = -1;
                    previousMouseY = -1;
                } else if (keyPressed->code == sf::Keyboard::Key::Space &&
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

            if (const auto* wheelScrolled =
                    event.getIf<sf::Event::MouseWheelScrolled>()) {
                if (wheelScrolled->wheel == sf::Mouse::Wheel::Vertical) {
                    zoomCameraAtPixel(
                        camera, grid, window, wheelScrolled->position,
                        wheelScrolled->delta
                    );
                    previousMouseX = -1;
                    previousMouseY = -1;
                }
            }

            if (const auto* mousePressed =
                    event.getIf<sf::Event::MouseButtonPressed>()) {
                if (mousePressed->button == sf::Mouse::Button::Middle &&
                    pixelIsInsideBoard(
                        window, camera, mousePressed->position
                    )) {
                    middleDragging = true;
                    previousPanPixel = mousePressed->position;
                    previousMouseX = -1;
                    previousMouseY = -1;
                } else if (mousePressed->button == sf::Mouse::Button::Left) {
                    sf::Vector2f uiPosition = window.mapPixelToCoords(
                        mousePressed->position, uiView
                    );
                    ComparisonPanelAction action = comparisonPanelActionAt(
                        uiPosition
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

            if (const auto* mouseReleased =
                    event.getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseReleased->button == sf::Mouse::Button::Middle) {
                    middleDragging = false;
                }
            }

            if (const auto* mouseMoved =
                    event.getIf<sf::Event::MouseMoved>()) {
                if (middleDragging) {
                    bool previousInside = pixelIsInsideBoard(
                        window, camera, previousPanPixel
                    );
                    bool currentInside = pixelIsInsideBoard(
                        window, camera, mouseMoved->position
                    );
                    if (previousInside && currentInside) {
                        sf::Vector2f previousWorld = window.mapPixelToCoords(
                            previousPanPixel, camera.worldView
                        );
                        sf::Vector2f currentWorld = window.mapPixelToCoords(
                            mouseMoved->position, camera.worldView
                        );
                        camera.worldView.move(previousWorld - currentWorld);
                        clampCameraToMap(camera, grid);
                    }
                    previousPanPixel = mouseMoved->position;
                }
            }
        }

        if (!window.isOpen()) break;

        if (appScreen == MAP_SELECTION && startMapRequested) {
            loadMap(
                activeMap, mapConfigForSelection(mapSelection), camera,
                gameState, comparisonResults, animation, playerPathLength,
                previousMouseX, previousMouseY, previousEditType,
                middleDragging
            );
            appScreen = GAME;
        }

        if (appScreen == GAME && returnToSelectionRequested) {
            unloadMap(
                activeMap, gameState, comparisonResults, animation,
                playerPathLength, previousMouseX, previousMouseY,
                previousEditType, middleDragging
            );
            appScreen = MAP_SELECTION;
        }

        if (appScreen == GAME && regenerateRequested) {
            MapConfig replacementConfig = activeMap.config;
            if (replacementConfig.type != CLASSIC_MAP) {
                replacementConfig.seed = randomSeedDifferentFrom(
                    replacementConfig.seed
                );
                mapSelection.seed = replacementConfig.seed;
            } else {
                replacementConfig = classicMapConfig();
            }
            loadMap(
                activeMap, replacementConfig, camera, gameState,
                comparisonResults, animation, playerPathLength,
                previousMouseX, previousMouseY, previousEditType,
                middleDragging
            );
            compareRequested = false;
            cancelRequested = false;
            resetRequested = false;
            clearRequested = false;
        }

        if (appScreen == MAP_SELECTION) {
            animation.frameClock.restart();
            window.clear(sf::Color(9, 14, 25));
            window.setView(uiView);
            sf::Vector2f mousePosition = window.mapPixelToCoords(
                sf::Mouse::getPosition(window), uiView
            );
            drawMapSelection(window, font, mapSelection, mousePosition);
            window.display();
            continue;
        }

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

        if (editingEnabled && !middleDragging &&
            (leftPressed || rightPressed)) {
            sf::Vector2i pos = sf::Mouse::getPosition(window);
            NodeType editType = rightPressed ? EMPTY : PLAYER_PATH;
            std::optional<GridPosition> gridPosition = gridPositionAtPixel(
                window, camera, grid, pos
            );

            if (gridPosition.has_value()) {
                int x = gridPosition->x;
                int y = gridPosition->y;
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
        window.setView(camera.worldView);
        for (const auto& row : grid) {
            for (GridNode* node : row) {
                window.draw(node->shape);
            }
        }
        if (gameState == COMPARISON_COMPLETE ||
            gameState == NO_PATH_RESULT) {
            drawComparisonPaths(window, comparisonResults);
        }
        window.setView(uiView);
        sf::Vector2f mousePosition = window.mapPixelToCoords(
            sf::Mouse::getPosition(window), uiView
        );
        drawComparisonPanel(
            window, font, activeMap.config, gameState, playerPathLength,
            comparisonResults, animation, mousePosition
        );
        window.display();

    }

    return 0;
}
