#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Combat/BattlefieldCreator/BattlefieldCreatorView.hpp"
#include <memory>
#include <string>
#include <vector>

enum class BattlefieldTerrainTool
{
    Inspect,
    Paint,
    Erase
};

class BattlefieldCreatorController : public IWidgetController
{
public:
    explicit BattlefieldCreatorController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;

private:
    friend class BattlefieldCreatorView;
    friend struct SecondaryEditorPersistenceProbe;

    struct UndoSnapshot
    {
        EncounterModel battlefield;
        std::string label;
    };

    static constexpr std::size_t maxUndoHistory = 24;

    std::unique_ptr<BattlefieldCreatorView> _view;
    Campaign &_currentCampaign;
    EncounterModel _draftBattlefield;
    int _selectedBattlefieldIndex = -1;
    int _selectedMapIndex = 0;
    int _selectedTerrainIndex = 0;
    int _brushSize = 1;
    float _zoom = 1.0f;
    BattlefieldTerrainTool _terrainTool = BattlefieldTerrainTool::Paint;
    std::vector<UndoSnapshot> _undoHistory;
    bool _hasUndo = false;
    bool _strokeInProgress = false;
    bool _propertyEditUndoActive = false;
    std::string _undoLabel;
    bool _saveFailed = false;
    std::string _saveErrorMessage;
    EntityId _draftCampaignId;

    void startNewBattlefield();
    void loadSelectedBattlefield();
    void saveBattlefield();
    void generateFromPartyLocation();
    void useSelectedMapAsBattlefield();
    bool selectedMapCanBeBattlefield() const;
    void generateTerrain(MapTileKind sourceTerrain);
    void generateTerrainInternal(MapTileKind sourceTerrain);
    void fillTerrain(MapTileKind terrain);
    void fillTerrainInternal(MapTileKind terrain);
    void paintCell(int x, int y);
    void endTerrainStroke();
    void captureUndoSnapshot(const std::string &label);
    void beginPropertyEditUndo(const std::string &label);
    void finishPropertyEditUndo();
    void refreshUndoState();
    void clearUndoHistory();
    void undoLastAction();
    MapTileKind selectedTerrain() const;
    void cancelTerrainTool();
    MapModel *getSelectedMap();
    const MapModel *getSelectedMap() const;
    std::vector<MapTileKind> getTerrainPalette() const;
    bool saveCampaign();
    void onShow() override;
};
