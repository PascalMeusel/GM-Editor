#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace gm::ux
{
enum class WorkspaceTarget
{
    Home,
    Story,
    Cast,
    World,
    Encounters,
    Compendium,
    Live,
    Settings
};

// Contextual adapters are temporary bridges to an existing controller while its
// replacement reaches parity. RetiredNavigation entries may retain models and
// controller code, but must never be offered as independent destinations.
enum class LegacySurfaceDisposition
{
    ContextualAdapter,
    RetiredNavigation,
    DetachedOutput
};

enum class LegacyEntryPoint
{
    PrimaryNavigation,
    WorkspaceContext,
    UniversalSearch
};

struct LegacySurfaceRegistration
{
    std::string_view routeId;
    std::string_view legacyName;
    std::string_view controllerType;
    WorkspaceTarget replacementWorkspace;
    LegacySurfaceDisposition disposition;
    std::string_view replacementMode;
    std::string_view parityGate;
    bool persistedDataPreserved = true;
};

struct LegacySurfaceRegistryValidation
{
    bool valid = true;
    std::vector<std::string> errors;
};

const std::vector<LegacySurfaceRegistration> &legacySurfaceRegistry();
const LegacySurfaceRegistration *findLegacySurface(std::string_view routeId);
std::vector<const LegacySurfaceRegistration *> legacySurfacesFor(WorkspaceTarget workspace);
bool isLegacyEntryPointAllowed(const LegacySurfaceRegistration &surface, LegacyEntryPoint entryPoint);
bool isLegacyEntryPointAllowed(std::string_view routeId, LegacyEntryPoint entryPoint);
LegacySurfaceRegistryValidation validateLegacySurfaceRegistry();
std::string_view workspaceTargetName(WorkspaceTarget workspace);
}
