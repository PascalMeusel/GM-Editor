#include "Application.hpp"
#include "Models/Character/PortraitRecipe.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Views/Editor/Widgets/UiInteractionTrace.hpp"
#include "fixtures/DeterministicSessionFixture.hpp"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
struct ApplicationQaHarness
{
    static GLFWwindow *window(Application &app)
    {
        return app.mainWindow.get();
    }
    static EditorWindowController &editor(Application &app)
    {
        return *app.editor;
    }
    static void load(Application &app, Campaign campaign)
    {
        app.editor->setCurrentCampaign(std::move(campaign));
        app.activeWindowController = app.editor;
        app.editor->setWindowOpacity(true);
    }
};
struct ActorDraftWorkflowProbe
{
    static void kind(NPCCreatorController &actor, int category)
    {
        actor._selectedEntryType = category == 4 ? 1 : 0;
        actor._selectedNpcCategory = category == 4 ? 0 : category;
    }
};
struct SpellWorkflowProbe
{
    static void open(SpellLibraryController &spells)
    {
        spells._view->requestEditorTab();
    }
};
struct WorkspaceInteractionProbe
{
    static void seedActor(EditorWindowController &editor, int category)
    {
        auto &actor = *editor._npcCreator;
        ActorDraftWorkflowProbe::kind(actor, category);
        actor.getModel().name = category == 2 ? "The Reedwardens" : category == 3 ? "The Keeper Below" : "Mara Voss";
        actor.getModel().role = category == 2 ? "Guardians of the marsh" : "Keeper of the crossing";
        actor.getModel().description = "A watchful guide who remembers every traveller and every broken promise.";
        actor.getMonsterModel().name = "Marsh stalker";
        actor.getMonsterModel().type = "Beast";
        gm::portraits::Recipe portrait;
        portrait.species = category == 4 ? 5 : 1;
        portrait.features = 1;
        portrait.expression = category == 4 ? 2 : 0;
        gm::portraits::write(category == 4 ? actor.getMonsterModel().metadata : actor.getModel().metadata, portrait);
    }
    static void seedPlayer(EditorWindowController &editor)
    {
        auto &p = editor._playerCreator->getModel();
        gm::portraits::Recipe r;
        r.species = 2;
        gm::portraits::write(p.metadata, r);
    }
    static void spell(EditorWindowController &editor)
    {
        SpellWorkflowProbe::open(*editor._spellLibrary);
    }
    static void seedItem(EditorWindowController &editor)
    {
        auto &item = editor._itemCreator->getModel();
        item.name = "Lantern of low tides";
        item.itemType = "Gear";
        item.description = "A brass lantern with sea-green glass. Its light reveals the old paths beneath the water.";
        item.weight = 2;
        item.baseValue = 7500;
    }
};
namespace
{
void write32(std::ostream &out, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        out.put(static_cast<char>(value >> shift));
}
void write16(std::ostream &out, std::uint16_t value)
{
    out.put(static_cast<char>(value));
    out.put(static_cast<char>(value >> 8));
}
void capture(Application &app, const std::filesystem::path &output, GLFWwindow *target = nullptr)
{
    std::filesystem::create_directories(output);
    auto *window = target ? target : ApplicationQaHarness::window(app);
    struct RestoreContext
    {
        GLFWwindow *previous = glfwGetCurrentContext();
        ~RestoreContext()
        {
            glfwMakeContextCurrent(previous);
        }
    } restoreContext;
    glfwMakeContextCurrent(window);
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0)
        throw std::runtime_error("No framebuffer");
    const std::size_t stride = (static_cast<std::size_t>(width) * 3 + 3) & ~std::size_t(3);
    std::vector<unsigned char> pixels(stride * height);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(target ? GL_FRONT : GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("Framebuffer read failed");
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            std::swap(pixels[y * stride + x * 3], pixels[y * stride + x * 3 + 2]);
    std::ofstream bitmap(output / "capture.bmp", std::ios::binary);
    bitmap.write("BM", 2);
    write32(bitmap, static_cast<std::uint32_t>(54 + pixels.size()));
    write32(bitmap, 0);
    write32(bitmap, 54);
    write32(bitmap, 40);
    write32(bitmap, width);
    write32(bitmap, height);
    write16(bitmap, 1);
    write16(bitmap, 24);
    write32(bitmap, 0);
    write32(bitmap, static_cast<std::uint32_t>(pixels.size()));
    write32(bitmap, 2835);
    write32(bitmap, 2835);
    write32(bitmap, 0);
    write32(bitmap, 0);
    bitmap.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
    if (!bitmap)
        throw std::runtime_error("Framebuffer write failed");
    std::ofstream report(output / "capture.txt");
    float xscale = 0, yscale = 0;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    report << "framebuffer=" << width << 'x' << height << "\ncontent_scale=" << xscale << ',' << yscale
           << "\nvendor=" << reinterpret_cast<const char *>(glGetString(GL_VENDOR))
           << "\nrenderer=" << reinterpret_cast<const char *>(glGetString(GL_RENDERER))
           << "\nversion=" << reinterpret_cast<const char *>(glGetString(GL_VERSION)) << '\n';
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
            throw std::runtime_error("Expected an isolated capture directory and optional locale / width / height");
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        const bool german = argc > 2 && std::string(argv[2]) == "de-DE";
        const std::vector<std::string> cases = {
            "person",  "monster",   "faction",   "deity", "player", "item",  "spell",           "map-setup",
            "dungeon", "encounter", "inventory", "shop",  "audio",  "story", "character-sheet", "handout"};
        ApplicationRunOptions options;
        options.width = argc > 3 ? std::stoi(argv[3]) : 1280;
        options.height = argc > 4 ? std::stoi(argv[4]) : 900;
        if (options.width < 1280 || options.height < 720)
            throw std::runtime_error("Requested viewport is below the supported 1280x720 application minimum");
        options.showWindow = false;
        options.maximizeOnStart = false;
        options.maximumFrames = cases.size() * 25 + 2;
        options.initialized = [&](Application &app) {
            gm::localization::LocalizationService::instance().setPreference(
                german ? gm::localization::LocalePreference::German : gm::localization::LocalePreference::English,
                false);
            auto campaign = gm::qa::threeSceneCampaign(german);
            for (auto &character : campaign.characters)
            {
                gm::portraits::Recipe r;
                r.species = 2;
                gm::portraits::write(character.metadata, r);
            }
            DungeonModel dungeon;
            dungeon.name = "Drowned chapel";
            if (!campaign.maps.empty())
                dungeon.parentMapId = campaign.maps.front().metadata.id;
            for (int i = 0; i < 3; ++i)
            {
                DungeonRoomModel room;
                room.name = i == 0 ? "Entry" : i == 1 ? "Flooded nave" : "Reliquary";
                room.keyedDescription = "Cold water gathers beneath a broken stone arch.";
                room.readAloudText = "A distant bell sounds through the flooded hall.";
                room.mapX = 3 + i * 5;
                room.mapY = 3 + (i % 2) * 4;
                dungeon.rooms.push_back(room);
            }
            for (int i = 0; i < 2; ++i)
            {
                DungeonRoomConnectionModel c;
                c.targetRoomId = dungeon.rooms[i + 1].metadata.id;
                dungeon.rooms[i].connections.push_back(c);
            }
            campaign.dungeons.push_back(dungeon);
            ShopModel shop;
            shop.name = "The Copper Lantern";
            shop.merchantName = "Mara Voss";
            shop.description = "Travel supplies, lantern oil, and local maps.";
            campaign.shops.push_back(shop);
            CampaignRepository::saveCampaign(campaign);
            ApplicationQaHarness::load(app, std::move(campaign));
            auto &save = ProjectSaveService::getInstance();
            save.setBackupSnapshotsEnabled(false);
            save.setRecoverySnapshotsEnabled(false);
            ImGui::GetPlatformIO().Platform_ShowWindow = [](ImGuiViewport *) {};
        };
        options.beforeFrame = [&](Application &app, std::uint64_t frame) {
            ImGui::GetIO().DeltaTime = 1.f / 60.f;
            if (frame % 25 != 1)
                return;
            const auto index = frame / 25;
            if (index >= cases.size())
                return;
            auto &editor = ApplicationQaHarness::editor(app);
            switch (index)
            {
            case 0:
            case 1:
            case 2:
            case 3:
                editor.showNPCCreator();
                WorkspaceInteractionProbe::seedActor(editor, index == 1 ? 4 : index == 2 ? 2 : index == 3 ? 3 : 0);
                break;
            case 4:
                editor.showPlayerCreator();
                WorkspaceInteractionProbe::seedPlayer(editor);
                break;
            case 5:
                editor.showItemCreator();
                WorkspaceInteractionProbe::seedItem(editor);
                break;
            case 6:
                editor.showSpellLibrary();
                WorkspaceInteractionProbe::spell(editor);
                break;
            case 7:
                editor.showMapCreator();
                break;
            case 8:
                editor.showDungeonBuilder();
                break;
            case 9:
                editor.showEncounterBuilder();
                break;
            case 10:
                editor.showInventoryManager();
                break;
            case 11:
                editor.showShopManager();
                break;
            case 12:
                editor.showAudioManager();
                break;
            case 13:
                editor.showStoryWorkspace();
                break;
            case 14:
                editor.showCharacterSheet();
                break;
            case 15:
                editor.showHandoutCreator();
                break;
            }
        };
        options.afterRender = [&](Application &app, std::uint64_t frame) {
            const auto index = frame / 25;
            if (frame % 25 == 22 && index < cases.size())
                capture(app, output / cases[index]);
        };
        auto app = std::make_shared<Application>(std::move(options));
        app->run();
        std::ofstream report(output / "gallery.txt");
        report << "Production workspace captures; QA-directed navigation. No reference approval asserted.\n";
        for (const auto &name : cases)
        {
            if (!std::filesystem::exists(output / name / "capture.bmp"))
                throw std::runtime_error("Missing capture: " + name);
            report << name << "\n";
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
