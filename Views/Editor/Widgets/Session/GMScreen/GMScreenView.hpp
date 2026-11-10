#pragma once

#include "Models/Inventory/InventoryModels.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Session/LocalPlayerDisplayModel.hpp"
#include "Models/Session/PlayerReviewSession.hpp"
#include "Views/Editor/Widgets/DockableWidget.hpp"

#include <vector>

class GMScreenController;

enum class PublicDisplayReviewAction
{
    None,
    Arm,
    Publish
};

class GMScreenView : public DockableWidget
{
public:
    explicit GMScreenView(GMScreenController &controller);
    void resetPlayerReview(bool clearRestore=false) {
        ++_playerReviewContextEpoch;
        _playerReviewSession.clear(); _publicDisplayReviewAction = PublicDisplayReviewAction::None; _reviewError.clear();
        if(clearRestore){_hasPublicDisplayRestoreSnapshot=false;_publicDisplayRestoreSnapshot={};}
    }
    void invalidatePlayerReview(const PlayerDisplayReviewSnapshot &review) {
        if(&review==&_playerReviewSession.review())_playerReviewSession.invalidate();
    }
    std::shared_ptr<gm::map_render::MapRenderCompileMemo> compileMemoFor(const PlayerDisplayReviewSnapshot &review) const {
        return &review==&_playerReviewSession.review()?_playerReviewSession.compileMemo():nullptr;
    }
    void pollPlayerReview();
    void finishPlayerReviewFrame();
    bool rejectUnreadyPlayerReview(const PlayerDisplayReviewSnapshot &review);

protected:
    void draw() override;

private:
    friend struct SessionTargetProbe;
    friend struct PlayerReviewUiProbe;
    GMScreenController &_controller;
    LocalPlayerDisplayModel _publicDisplayRestoreSnapshot;
    bool _hasPublicDisplayRestoreSnapshot = false;
    int _pendingHandoutRemovalIndex = -1;
    int _pendingRulesBookmarkRemovalIndex = -1;
    PublicDisplayReviewAction _publicDisplayReviewAction = PublicDisplayReviewAction::None;
    PlayerReviewSession _playerReviewSession;
    std::uint64_t _playerReviewContextEpoch=0;
    int _playerReviewDrawFrame=-1;
    std::string _reviewError;

    void drawTableCoreDashboard(const std::vector<ItemModel> &items);
    void drawPublicDisplayTab();
    void drawMagicEffectsTab();
    void drawHandoutsTab();
    void drawRandomRulesTab();
    void drawAdvancedTab();
    void drawSessionStatusStrip(const char *id) const;
    void drawPublicDisplayStatusStrip(const char *id) const;
    void drawSessionFlow();
    void drawLocalPlayerDisplayControls();
    void preparePublicDisplayReview(PublicDisplayReviewAction action);
    void drawPublicDisplayReviewModal(const char *popupId);
    void drawHandoutRevealControls();
    void drawPartyPassives(const std::vector<ItemModel> &items);
    void drawCommonDcs();
    void drawConditionsSnapshot();
    void drawLiveSpellSlots();
    void drawConditionsReference();
    void drawQuickNpcs();
    void drawRandomTools();
    void drawRulesBookmarks();
    void drawActiveEffects();
};
