#include "ApplicationInternal.hpp"

using namespace gm::application_detail;

namespace gm::application_detail
{
std::atomic<StartupDiagnostics *> activeStartupDiagnostics{nullptr};
std::atomic<unsigned int> recordedGlfwErrors{0};

void recordGlfwError(int code, const char *description)
{
    if (StartupDiagnostics *diagnostics = activeStartupDiagnostics.load())
    {
        try
        {
            const unsigned int count = recordedGlfwErrors.fetch_add(1);
            if (count < 16)
                diagnostics->record("glfw.error", std::to_string(code) + " " + (description ? description : ""));
            else if (count == 16)
                diagnostics->record("glfw.error", "further GLFW errors suppressed for this launch");
        }
        catch (...)
        {
        }
    }
}

void ensureMainContext(GLFWwindow *window, StartupDiagnostics &diagnostics)
{
    if (glfwGetCurrentContext() == window)
        return;
    glfwMakeContextCurrent(window);
    if (glfwGetCurrentContext() != window)
    {
        diagnostics.record("opengl.context", "could not make main context current");
        throw std::runtime_error("Grafikkontext konnte nicht aktiviert werden / Failed to activate the graphics context");
    }
}

std::string windowPresentationState(GLFWwindow *window)
{
    return "visible=" + std::to_string(glfwGetWindowAttrib(window, GLFW_VISIBLE)) +
           " iconified=" + std::to_string(glfwGetWindowAttrib(window, GLFW_ICONIFIED)) +
           " maximized=" + std::to_string(glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) +
           " focused=" + std::to_string(glfwGetWindowAttrib(window, GLFW_FOCUSED)) +
           " context_current=" + std::to_string(glfwGetCurrentContext() == window);
}

void applyLockedShellDockFlags(ImGuiDockNode *node)
{
    if (!node)
        return;
    node->SetLocalFlags(node->LocalFlags | kLockedShellDockFlags);
}

void applyEditorShellRootDockFlags(ImGuiID dockspaceId)
{
    ImGuiDockNode *root = ImGui::DockBuilderGetNode(dockspaceId);
    if (!root)
        return;

    root->SetLocalFlags(root->LocalFlags | kEditorShellRootDockFlags);
}

void collectDockLeaves(ImGuiDockNode *node, std::vector<ImGuiDockNode *> &leaves)
{
    if (!node)
        return;
    if (node->IsLeafNode())
    {
        leaves.push_back(node);
        return;
    }
    collectDockLeaves(node->ChildNodes[0], leaves);
    collectDockLeaves(node->ChildNodes[1], leaves);
}

std::vector<ImGuiDockNode *> utilityRailLeaves(ImGuiID dockspaceId)
{
    ImGuiDockNode *root = ImGui::DockBuilderGetNode(dockspaceId);
    std::vector<ImGuiDockNode *> leaves;
    collectDockLeaves(root, leaves);

    std::vector<ImGuiDockNode *> lockedLeaves;
    for (ImGuiDockNode *leaf : leaves)
    {
        if (!leaf || leaf->IsCentralNode())
            continue;
        if ((leaf->LocalFlags & kLockedShellDockFlags) == kLockedShellDockFlags)
            lockedLeaves.push_back(leaf);
    }

    std::sort(lockedLeaves.begin(), lockedLeaves.end(), [](const ImGuiDockNode *lhs, const ImGuiDockNode *rhs) {
        if (lhs->Pos.x != rhs->Pos.x)
            return lhs->Pos.x < rhs->Pos.x;
        if (lhs->Pos.y != rhs->Pos.y)
            return lhs->Pos.y < rhs->Pos.y;
        return lhs->ID < rhs->ID;
    });

    if (lockedLeaves.size() < 4)
        return {};

    std::vector<ImGuiDockNode *> utilityLeaves(lockedLeaves.end() - 3, lockedLeaves.end());
    std::sort(utilityLeaves.begin(), utilityLeaves.end(), [](const ImGuiDockNode *lhs, const ImGuiDockNode *rhs) {
        if (lhs->Pos.y != rhs->Pos.y)
            return lhs->Pos.y < rhs->Pos.y;
        return lhs->ID < rhs->ID;
    });
    return utilityLeaves;
}

ImGuiDockNode *sidebarShellLeaf(ImGuiID dockspaceId)
{
    ImGuiDockNode *root = ImGui::DockBuilderGetNode(dockspaceId);
    std::vector<ImGuiDockNode *> leaves;
    collectDockLeaves(root, leaves);

    ImGuiDockNode *leftmostLockedLeaf = nullptr;
    for (ImGuiDockNode *leaf : leaves)
    {
        if (!leaf || leaf->IsCentralNode())
            continue;
        if ((leaf->LocalFlags & kLockedShellDockFlags) != kLockedShellDockFlags)
            continue;
        if (!leftmostLockedLeaf ||
            leaf->Pos.x < leftmostLockedLeaf->Pos.x ||
            (leaf->Pos.x == leftmostLockedLeaf->Pos.x && leaf->ID < leftmostLockedLeaf->ID))
            leftmostLockedLeaf = leaf;
    }

    return leftmostLockedLeaf;
}

void dockUtilityShellWindows(ImGuiID gameTimeDockId, ImGuiID audioDockId, ImGuiID miniMapDockId)
{
    applyLockedShellDockFlags(ImGui::DockBuilderGetNode(gameTimeDockId));
    applyLockedShellDockFlags(ImGui::DockBuilderGetNode(audioDockId));
    applyLockedShellDockFlags(ImGui::DockBuilderGetNode(miniMapDockId));

    ImGui::DockBuilderDockWindow("Game Time", gameTimeDockId);
    ImGui::DockBuilderDockWindow("Audio Mini Player", audioDockId);
    ImGui::DockBuilderDockWindow("Mini Map", miniMapDockId);
}

ImGuiID buildEditorShellDockLayout(ImGuiID dockspaceId, const ImVec2 &dockspaceSize, bool includeUtilityRail)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, dockspaceSize);

    ImGuiID selectorDockId = 0;
    ImGuiID remainingDockId = 0;
    ImGuiID workDockId = 0;
    const float navigationWidth = std::clamp(190.0f * ImGui::GetFontSize() / 16.0f, 160.0f, std::max(160.0f, dockspaceSize.x * 0.22f));
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, navigationWidth / std::max(1.0f, dockspaceSize.x), &selectorDockId, &remainingDockId);

    applyLockedShellDockFlags(ImGui::DockBuilderGetNode(selectorDockId));
    DockableWidget::setSidebarDockId(selectorDockId);

    if (includeUtilityRail)
    {
        ImGuiID utilityDockId = 0;
        ImGuiID gameTimeDockId = 0;
        ImGuiID utilityRestDockId = 0;
        ImGuiID audioDockId = 0;
        ImGuiID miniMapDockId = 0;
        ImGui::DockBuilderSplitNode(remainingDockId, ImGuiDir_Right, 0.24f, &utilityDockId, &workDockId);
        ImGui::DockBuilderSplitNode(utilityDockId, ImGuiDir_Up, 0.22f, &gameTimeDockId, &utilityRestDockId);
        ImGui::DockBuilderSplitNode(utilityRestDockId, ImGuiDir_Up, 0.32f, &audioDockId, &miniMapDockId);
        dockUtilityShellWindows(gameTimeDockId, audioDockId, miniMapDockId);
    }
    else
    {
        workDockId = remainingDockId;
    }

    ImGui::DockBuilderDockWindow("Window Selector", selectorDockId);
    ImGui::DockBuilderDockWindow("Workflow Sidebar###Window Selector", selectorDockId);
    ImGui::DockBuilderDockWindow("Campaign Home", workDockId);
    ImGui::DockBuilderFinish(dockspaceId);
    applyEditorShellRootDockFlags(dockspaceId);
    return workDockId;
}
} // namespace gm::application_detail
