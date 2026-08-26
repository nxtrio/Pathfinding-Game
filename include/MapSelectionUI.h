#ifndef MAPSELECTIONUI_H
#define MAPSELECTIONUI_H

#include "MapGeneration.h"

enum MapSizePreset { SMALL_MAP_SIZE, MEDIUM_MAP_SIZE, LARGE_MAP_SIZE };

struct MapDimensions {
    int rows;
    int cols;
};

struct MapSelectionState {
    MapType selectedType = CLASSIC_MAP;
    MapSizePreset selectedSize = MEDIUM_MAP_SIZE;
    unsigned int seed = 482917u;
};

enum MapSelectionAction {
    NO_MAP_SELECTION_ACTION,
    SELECT_CLASSIC_ACTION,
    SELECT_GROWING_TREE_ACTION,
    SELECT_RECURSIVE_DIVISION_ACTION,
    SELECT_SMALL_SIZE_ACTION,
    SELECT_MEDIUM_SIZE_ACTION,
    SELECT_LARGE_SIZE_ACTION,
    RANDOMIZE_SEED_ACTION,
    START_MAP_ACTION
};

MapDimensions dimensionsForPreset(MapSizePreset preset);
MapConfig mapConfigForSelection(const MapSelectionState& selection);
MapSelectionAction mapSelectionActionAt(sf::Vector2f position);

void drawMapSelection(sf::RenderTarget& target,
                      const sf::Font& font,
                      const MapSelectionState& selection,
                      sf::Vector2f mousePosition);

#endif
