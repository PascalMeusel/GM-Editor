#pragma once

#include "Views/Editor/Widgets/DockableWidget.hpp"
#include "Models/Map/MapRenderPlanCache.hpp"
#include "Models/Combat/EncounterTypes.hpp"
#include <string>

class BattleTrackerController;

class BattleTrackerView : public DockableWidget
{
public:
    explicit BattleTrackerView(BattleTrackerController &controller);
    void drawEmbedded();

protected:
    void draw() override;

private:
    enum class CombatControlsFocus
    {
        None,
        ActionSelection,
        ActionTargets,
        Effects
    };

    BattleTrackerController &_controller;
    MapRenderPlanCache _battleRenderPlanCache{4};
    EntityId _battleCanvasEncounterId;
    int _battleCanvasWidth = 0;
    int _battleCanvasHeight = 0;
    ImVec2 _battleCanvasViewport{};
    bool _battleCanvasAutoFit = true;
    bool _focusWrapUp = false;
    bool _wrapUpNotesDirty = false;
    EntityId _wrapUpNotesEncounterId;
    std::string _lastVisibleCombatStatus;
    double _combatStatusChangedAt = 0.0;
    CombatControlsFocus _pendingCombatControlsFocus = CombatControlsFocus::None;
    bool _compactBattlefield = false;
    EntityId _quickCorrectionEncounterId;
    EntityId _quickCorrectionParticipantId;

    void drawSetupPanel();
    void drawParticipantPanel();
    void drawSelectedCombatantPanel();
    void drawTurnActions();
    void drawPersistentCombatNavigation();
    void drawQuickCorrections();
    void drawEffectsReviewPanel();
    void drawRangePanel();
    void drawDiceRollPanel();
    void drawCombatLogPanel();
    void drawWrapUpPanel();
    void drawBattlefield();
    void drawAttackTotalSummary(const char *bonusLabel) const;
    int adjudicationModifierCount() const;
    std::string adjudicationSummaryText() const;
    std::string adjudicationSummaryTooltip() const;
    void changeCombatPhaseAndFocus(CombatPhase phase);
    bool hasCombatControlsFocus(CombatControlsFocus focus) const;
    bool consumeCombatControlsFocus(CombatControlsFocus focus);
};
