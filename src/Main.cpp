#include "PathfindingGame.h"
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <queue>

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

// BFS that walks ONLY along hand-drawn path cells (START/WALL/END).
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
        return (n->type == START || n->type == WALL || n->type == END);
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

int main() {
    // SFML 3: VideoMode takes a Vector2u
    sf::RenderWindow window(
        sf::VideoMode({static_cast<unsigned int>(COL_COUNT * CELL_SIZE),
                       static_cast<unsigned int>(ROW_COUNT * CELL_SIZE)}),
        "Pathfinding Game"
    );
    window.setFramerateLimit(60);

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
    sf::Font font;
    if (!font.openFromFile("assets/arial.ttf")) {
        std::cerr << "Failed to load font for HUD (assets/arial.ttf)\n";
    }

    sf::Text pathText(font, "", 20);
    pathText.setFillColor(sf::Color::White);
    pathText.setPosition(sf::Vector2f(10.f, 10.f));
    pathText.setString("Path length: N/A");

    bool shouldClose = false;

    while (window.isOpen()) {
        // --- Events ---
        while (const std::optional eventOpt = window.pollEvent()) {
            const sf::Event& event = *eventOpt;

            // Window closed
            if (event.is<sf::Event::Closed>()) {
                window.close();
                continue;
            }

            // Left Click: Draw Wall (but not on START/END/OBSTACLE)
            if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                sf::Vector2i pos = sf::Mouse::getPosition(window);
                int x = pos.x / CELL_SIZE;
                int y = pos.y / CELL_SIZE;

                if (x >= 0 && x < COL_COUNT && y >= 0 && y < ROW_COUNT) {
                    GridNode* n = grid[y][x];
                    if (n != startNode && n != endNode && n->type != OBSTACLE) {
                        n->setType(WALL);
                    }
                }
            }

            // Right Click: Erase (but not START/END/OBSTACLE)
            if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
                sf::Vector2i pos = sf::Mouse::getPosition(window);
                int x = pos.x / CELL_SIZE;
                int y = pos.y / CELL_SIZE;

                if (x >= 0 && x < COL_COUNT && y >= 0 && y < ROW_COUNT) {
                    GridNode* n = grid[y][x];
                    if (n != startNode && n != endNode && n->type != OBSTACLE) {
                        n->setType(EMPTY);
                    }
                }
            }
        }

        // --- Hand-drawn path detection ---
        int length = computeHandDrawnPathLength(grid, startNode, endNode);
        if (length >= 0) {
            pathText.setString("Path length: " + std::to_string(length));
            shouldClose = true;   // we’ll close after drawing this frame
        } else {
            pathText.setString("Path length: N/A");
            shouldClose = false;
        }

        // --- Render ---
        window.clear();
        for (int y = 0; y < ROW_COUNT; ++y) {
            for (int x = 0; x < COL_COUNT; ++x) {
                window.draw(grid[y][x]->shape);
            }
        }
        window.draw(pathText);
        window.display();

        if (shouldClose) {
            window.close();
        }
    }

    // Cleanup
    for (auto& row : grid) {
        for (auto& node : row) {
            delete node;
        }
    }

    return 0;
}
