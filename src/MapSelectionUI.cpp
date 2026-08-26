#include "MapSelectionUI.h"

#include <cmath>
#include <string>

namespace {

const sf::Color BACKGROUND(9, 14, 25);
const sf::Color CARD_BACKGROUND(23, 30, 47);
const sf::Color CARD_HOVER(29, 39, 59);
const sf::Color CARD_OUTLINE(49, 61, 82);
const sf::Color PRIMARY_TEXT(238, 242, 250);
const sf::Color SECONDARY_TEXT(151, 163, 184);
const sf::Color CLASSIC_ACCENT(145, 105, 220);
const sf::Color GROWING_ACCENT(45, 190, 235);
const sf::Color DIVISION_ACCENT(245, 157, 52);
const sf::Color BUTTON_BACKGROUND(39, 50, 70);
const sf::Color BUTTON_HOVER(56, 72, 99);

const sf::FloatRect CLASSIC_CARD(
    sf::Vector2f(80.f, 170.f), sf::Vector2f(350.f, 225.f)
);
const sf::FloatRect GROWING_CARD(
    sf::Vector2f(485.f, 170.f), sf::Vector2f(350.f, 225.f)
);
const sf::FloatRect DIVISION_CARD(
    sf::Vector2f(890.f, 170.f), sf::Vector2f(350.f, 225.f)
);

const sf::FloatRect SMALL_BUTTON(
    sf::Vector2f(448.f, 465.f), sf::Vector2f(135.f, 42.f)
);
const sf::FloatRect MEDIUM_BUTTON(
    sf::Vector2f(593.f, 465.f), sf::Vector2f(135.f, 42.f)
);
const sf::FloatRect LARGE_BUTTON(
    sf::Vector2f(738.f, 465.f), sf::Vector2f(135.f, 42.f)
);
const sf::FloatRect RANDOMIZE_BUTTON(
    sf::Vector2f(465.f, 565.f), sf::Vector2f(190.f, 45.f)
);
const sf::FloatRect START_BUTTON(
    sf::Vector2f(675.f, 565.f), sf::Vector2f(180.f, 45.f)
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

void drawCenteredText(sf::RenderTarget& target,
                      const sf::Font& font,
                      const std::string& value,
                      unsigned int size,
                      sf::Vector2f center,
                      sf::Color color = PRIMARY_TEXT) {
    sf::Text text(font, value, size);
    sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition(sf::Vector2f(
        std::round(center.x - bounds.size.x / 2.f - bounds.position.x),
        std::round(center.y - bounds.size.y / 2.f - bounds.position.y)
    ));
    text.setFillColor(color);
    target.draw(text);
}

void drawCard(sf::RenderTarget& target,
              const sf::Font& font,
              const sf::FloatRect& bounds,
              const std::string& title,
              const std::string& description,
              const std::string& detail,
              sf::Color accent,
              bool selected,
              bool hovered) {
    sf::RectangleShape card(bounds.size);
    card.setPosition(bounds.position);
    card.setFillColor(hovered ? CARD_HOVER : CARD_BACKGROUND);
    card.setOutlineColor(selected ? accent : CARD_OUTLINE);
    card.setOutlineThickness(selected ? 3.f : 1.f);
    target.draw(card);

    sf::RectangleShape accentBar(sf::Vector2f(bounds.size.x, 5.f));
    accentBar.setPosition(bounds.position);
    accentBar.setFillColor(accent);
    target.draw(accentBar);

    drawText(target, font, title, 21,
             bounds.position + sf::Vector2f(24.f, 28.f));
    drawText(target, font, description, 13,
             bounds.position + sf::Vector2f(24.f, 79.f), SECONDARY_TEXT);
    drawText(target, font, detail, 13,
             bounds.position + sf::Vector2f(24.f, 167.f), accent);
}

void drawButton(sf::RenderTarget& target,
                const sf::Font& font,
                const sf::FloatRect& bounds,
                const std::string& label,
                bool selected,
                bool hovered,
                sf::Color accent = GROWING_ACCENT) {
    sf::RectangleShape button(bounds.size);
    button.setPosition(bounds.position);
    button.setFillColor(hovered ? BUTTON_HOVER : BUTTON_BACKGROUND);
    button.setOutlineColor(selected ? accent : CARD_OUTLINE);
    button.setOutlineThickness(selected ? 2.f : 1.f);
    target.draw(button);
    drawCenteredText(target, font, label, 14, bounds.getCenter());
}

} // namespace

MapDimensions dimensionsForPreset(MapSizePreset preset) {
    switch (preset) {
        case SMALL_MAP_SIZE:
            return {21, 31};
        case MEDIUM_MAP_SIZE:
            return {35, 51};
        case LARGE_MAP_SIZE:
            return {55, 81};
    }
    return {35, 51};
}

MapConfig mapConfigForSelection(const MapSelectionState& selection) {
    if (selection.selectedType == CLASSIC_MAP) return classicMapConfig();

    MapDimensions dimensions = dimensionsForPreset(selection.selectedSize);
    if (selection.selectedType == GROWING_TREE_MAZE) {
        return growingTreeMapConfig(
            dimensions.rows, dimensions.cols, selection.seed
        );
    }
    return recursiveDivisionMapConfig(
        dimensions.rows, dimensions.cols, selection.seed
    );
}

MapSelectionAction mapSelectionActionAt(sf::Vector2f position) {
    if (CLASSIC_CARD.contains(position)) return SELECT_CLASSIC_ACTION;
    if (GROWING_CARD.contains(position)) return SELECT_GROWING_TREE_ACTION;
    if (DIVISION_CARD.contains(position)) {
        return SELECT_RECURSIVE_DIVISION_ACTION;
    }
    if (SMALL_BUTTON.contains(position)) return SELECT_SMALL_SIZE_ACTION;
    if (MEDIUM_BUTTON.contains(position)) return SELECT_MEDIUM_SIZE_ACTION;
    if (LARGE_BUTTON.contains(position)) return SELECT_LARGE_SIZE_ACTION;
    if (RANDOMIZE_BUTTON.contains(position)) return RANDOMIZE_SEED_ACTION;
    if (START_BUTTON.contains(position)) return START_MAP_ACTION;
    return NO_MAP_SELECTION_ACTION;
}

void drawMapSelection(sf::RenderTarget& target,
                      const sf::Font& font,
                      const MapSelectionState& selection,
                      sf::Vector2f mousePosition) {
    sf::RectangleShape background(sf::Vector2f(1320.f, 750.f));
    background.setFillColor(BACKGROUND);
    target.draw(background);

    drawCenteredText(target, font, "PATHFINDING LAB", 34,
                     sf::Vector2f(660.f, 61.f));
    drawCenteredText(
        target, font, "Choose a map to compare Dijkstra and A*", 16,
        sf::Vector2f(660.f, 111.f), SECONDARY_TEXT
    );

    MapSelectionAction hovered = mapSelectionActionAt(mousePosition);
    drawCard(
        target, font, CLASSIC_CARD, "CLASSIC",
        "The original staggered barrier\nlayout from the first version.",
        "40 x 30", CLASSIC_ACCENT,
        selection.selectedType == CLASSIC_MAP,
        hovered == SELECT_CLASSIC_ACTION
    );
    drawCard(
        target, font, GROWING_CARD, "GROWING TREE",
        "Newest-cell carving creates long,\nwinding passages.",
        "SEEDED PERFECT MAZE", GROWING_ACCENT,
        selection.selectedType == GROWING_TREE_MAZE,
        hovered == SELECT_GROWING_TREE_ACTION
    );
    drawCard(
        target, font, DIVISION_CARD, "RECURSIVE DIVISION",
        "Recursive walls create structured\nregions and bottlenecks.",
        "SEEDED PERFECT MAZE", DIVISION_ACCENT,
        selection.selectedType == RECURSIVE_DIVISION_MAZE,
        hovered == SELECT_RECURSIVE_DIVISION_ACTION
    );

    drawText(target, font, "MAP SIZE", 12, sf::Vector2f(343.f, 479.f),
             SECONDARY_TEXT);
    drawButton(target, font, SMALL_BUTTON, "SMALL  31 x 21",
               selection.selectedSize == SMALL_MAP_SIZE,
               hovered == SELECT_SMALL_SIZE_ACTION);
    drawButton(target, font, MEDIUM_BUTTON, "MEDIUM  51 x 35",
               selection.selectedSize == MEDIUM_MAP_SIZE,
               hovered == SELECT_MEDIUM_SIZE_ACTION);
    drawButton(target, font, LARGE_BUTTON, "LARGE  81 x 55",
               selection.selectedSize == LARGE_MAP_SIZE,
               hovered == SELECT_LARGE_SIZE_ACTION);

    std::string seedLabel = "SEED  " + std::to_string(selection.seed);
    drawCenteredText(target, font, seedLabel, 17,
                     sf::Vector2f(660.f, 541.f), SECONDARY_TEXT);
    drawButton(target, font, RANDOMIZE_BUTTON, "Randomize Seed", false,
               hovered == RANDOMIZE_SEED_ACTION);
    drawButton(target, font, START_BUTTON, "Start", true,
               hovered == START_MAP_ACTION, GROWING_ACCENT);

    if (selection.selectedType == CLASSIC_MAP) {
        drawCenteredText(
            target, font, "Classic uses its original dimensions; size and seed apply to mazes.",
            13, sf::Vector2f(660.f, 650.f), SECONDARY_TEXT
        );
    } else {
        MapDimensions dimensions = dimensionsForPreset(selection.selectedSize);
        drawCenteredText(
            target, font,
            std::to_string(dimensions.cols) + " x " +
                std::to_string(dimensions.rows) + " cells",
            13, sf::Vector2f(660.f, 650.f), SECONDARY_TEXT
        );
    }
}
