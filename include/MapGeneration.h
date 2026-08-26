#ifndef MAPGENERATION_H
#define MAPGENERATION_H

#include "PathfindingGame.h"

#include <string>
#include <utility>

enum MapType {
    CLASSIC_MAP,
    GROWING_TREE_MAZE,
    RECURSIVE_DIVISION_MAZE
};

struct MapConfig {
    MapType type = CLASSIC_MAP;
    std::string name;
    int rows = 0;
    int cols = 0;
    unsigned int seed = 0;
    GridPosition start{0, 0};
    GridPosition end{0, 0};
};

struct LoadedMap {
    Grid grid;
    GridNode* startNode = nullptr;
    GridNode* endNode = nullptr;
    MapConfig config;

    LoadedMap() = default;
    ~LoadedMap();

    LoadedMap(const LoadedMap&) = delete;
    LoadedMap& operator=(const LoadedMap&) = delete;
    LoadedMap(LoadedMap&& other) noexcept;
    LoadedMap& operator=(LoadedMap&& other) noexcept;
};

MapConfig classicMapConfig();
MapConfig growingTreeMapConfig(int rows, int cols, unsigned int seed);
MapConfig recursiveDivisionMapConfig(int rows, int cols, unsigned int seed);

void generateGrowingTreeMaze(Grid& grid, unsigned int seed);
void generateRecursiveDivisionMaze(Grid& grid, unsigned int seed);
std::pair<GridPosition, GridPosition> findDistantEndpoints(const Grid& grid);

LoadedMap createMap(const MapConfig& config);

#endif
