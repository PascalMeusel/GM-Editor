#pragma once

#include "Models/Map/MapModels.hpp"

namespace gm::map_creator_view_detail
{
struct WorkflowFrame final
{
    MapModel *map = nullptr;
    bool generationBusy = false;
    bool authoring = false;
    bool showIdea = false;
    bool showFacts = false;
    bool showDesigns = false;
    bool showEdit = false;
    bool showExport = false;
    bool showPlayerPreview = false;
    bool hasPersistentPrimaryAction = false;
    float persistentActionHeight = 0.0f;
    bool useStackedMapLayout = false;
};
}
