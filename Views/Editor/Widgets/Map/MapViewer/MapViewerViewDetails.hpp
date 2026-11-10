#pragma once

#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerView.hpp"
#include "Controllers/Editor/Widgets/MapViewer/MapViewerController.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapRuntimeVisualAssets.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/Map/MapFogRendering.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapFogOverlay.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorPreviewContract.hpp"
#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
