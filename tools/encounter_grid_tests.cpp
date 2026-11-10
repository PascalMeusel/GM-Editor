#include "Models/Combat/EncounterAtlasService.hpp"
#include "Models/Combat/EncounterMapRenderSource.hpp"
#include <iostream>

int main()
{
    int failures = 0;
    const auto check = [&](bool value, const char *message) {
        if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    MapModel source; source.metadata.id = "region"; source.atlasLevel = MapLevel::Region;
    MapModel map; map.metadata.id = "large-tactical"; map.atlasLevel = MapLevel::Tactical;
    map.mapKind = MapKind::Encounter; map.width = map.height = 80; map.cells.resize(6400);
    map.atlasPlacement.parentId = source.metadata.id;
    map.atlasPlacement.parentRect = {0, 0, 1, 1};
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 80; ++x)
        {
            auto &cell = map.cells[static_cast<std::size_t>(y) * 80 + x];
            cell.elevation = y * 100 + x;
            cell.fogged = (x + y) % 3 == 0;
            cell.movementBlockedMask = x % 4;
            cell.lineOfSightOneWayBlockedMask = y % 4;
        }
    EncounterModel encounter; encounter.cellSizeFeet = 5;
    EncounterParticipantModel participant; participant.x = participant.y = 79;
    participant.turnStartX = participant.turnStartY = 79; participant.startingX = participant.startingY = 79;
    encounter.participants.push_back(participant);
    std::vector<std::string> errors;
    check(EncounterAtlasService::transferToEncounter(source, map, encounter, &errors), "80x80 atlas transfer rejected");
    normalizeEncounter(encounter);
    check(encounter.battlefieldWidth == 80 && encounter.battlefieldHeight == 80 && encounter.battlefieldCells.size() == 6400,
          "normalization shrank80x80 battlefield");
    for (std::size_t i = 0; i < map.cells.size(); ++i)
        check(encounterMapRenderCellsMatch(map.cells[i], encounter.battlefieldCells[i]), "80x80 spatial cell data changed");
    check(encounter.participants.front().x == 79 && encounter.participants.front().turnStartY == 79, "edge token moved during normalization");
    encounter.cellSizeFeet = 10; normalizeEncounter(encounter);
    check(encounter.battlefieldWidth == 80 && encounter.battlefieldCells[79 * 80 + 79].elevation == 7979,
          "changing feet per cell must not resize/reinterpret terrain");
    const EncounterModel before = encounter;
    MapModel tooLarge = map; tooLarge.width = 513; tooLarge.cells.resize(513 * 80);
    errors.clear();
    check(!EncounterAtlasService::transferToEncounter(source, tooLarge, encounter, &errors) && !errors.empty(),
          "unsupported map must fail with reason before assignment");
    check(encounter.battlefieldWidth == before.battlefieldWidth && encounter.battlefieldCells.back().elevation == 7979,
          "rejected map changed existing encounter");
    MapModel incomplete = map; incomplete.cells.pop_back();
    check(!EncounterAtlasService::transferToEncounter(source, incomplete, encounter, &errors), "incomplete source grid accepted");
    resizeEncounterBattlefield(encounter, 96, 84);
    check(encounter.battlefieldCells[79 * 96 + 79].elevation == 7979 && encounter.battlefieldCells[80].elevation == 0,
          "expansion must preserve old x/y and initialize new columns");
    resizeEncounterBattlefield(encounter, 48, 60);
    check(encounter.battlefieldCells[59 * 48 + 47].elevation == 5947,
          "explicit crop must preserve retained cell coordinates");
    EncounterModel historical; historical.cellSizeFeet = 1; historical.battlefieldWidth = historical.battlefieldHeight = 328;
    historical.battlefieldCells.resize(328 * 328); historical.battlefieldCells.back().elevation = 876;
    normalizeEncounter(historical);
    check(historical.battlefieldWidth == 328 && historical.battlefieldCells.back().elevation == 876,
          "historically permitted1ft grid must remain intact");
    historical.cellSizeFeet = 5; normalizeEncounter(historical);
    check(historical.battlefieldWidth == 328 && historical.battlefieldCells.back().elevation == 876,
          "historical map scale edit must not destroy terrain");
    std::cout << "Encounter grid contracts: " << failures << " failures\n";
    return failures ? 1 : 0;
}
