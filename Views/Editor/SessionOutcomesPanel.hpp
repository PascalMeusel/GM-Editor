#pragma once
#include "Models/Session/SessionOutcomeService.hpp"
#include "Models/Session/SessionOutcomeDraftKeys.hpp"
#include "Models/Session/SessionOutcomeDraftState.hpp"
#include "Models/Session/PlayerRecapPackage.hpp"
#include "Models/Project/TextDraftRecovery.hpp"
#include "Views/Editor/SessionPreparationTarget.hpp"
#include <map>
#include <set>
#include <optional>
#include <cstdint>
#include <filesystem>

class SessionOutcomesPanel
{
public:
    SessionOutcomesPanel() = default;
    explicit SessionOutcomesPanel(SessionOutcomeDraftKeys::Surface surface) : _recoverySurface(surface) {}
    void drawLauncher(Campaign &, EntityId sessionId, const SessionPlan::Persist &, EntityId sceneId = {}, const PreparationTargetSink & = {});
    void draw(Campaign &, EntityId sessionId, const SessionPlan::Persist &, EntityId sceneId = {}, const PreparationTargetSink & = {});
    bool matchesPreparationTarget(const PreparationTargetRequest &) const;
    void reportPreparationTarget(const PreparationTargetRequest &, const PreparationTargetResult &);
    void clearPreparationTarget();
    bool checkpointDrafts(Campaign &);
    bool hasPendingDraftWrites() const;
    // Owners poll even when an inner tab is hidden, so failed writes retry at
    // the ordinary two-second cadence. Navigation uses checkpointDrafts.
    void tickDrafts(Campaign &);
private:
    SessionOutcomeDraftKeys::Surface _recoverySurface = SessionOutcomeDraftKeys::Surface::Preparation;
    std::string outcomeRecoveryKey(const EntityId &id) const { return SessionOutcomeDraftKeys::composer(_recoverySurface, id); }
    std::string recapRecoveryKey(const EntityId &id) const { return SessionOutcomeDraftKeys::recap(_recoverySurface, id); }
    struct Draft
    {
        SessionOutcomes::Outcome outcome;
        bool loaded = false, recovered = false, changed = false;
        bool recoveryPending = false, cleanupPending = false;
        // Once a person edits or restores a draft, checkpoints must never make
        // its source follow a later scene again.
        bool sourcePinned = false;
        std::optional<SessionOutcomes::Outcome> recoveredOutcome;
        std::string error;
        double lastCheckpoint = 0.0;
    };
    EntityId _campaignId, _recapSession, _drawnSession;
    std::map<EntityId, Draft> _drafts;
    using PublicDraft = SessionOutcomeDraftState::PublicText;
    std::map<EntityId, PublicDraft> _publicDrafts;
    struct RecapDraft
    {
        std::string title, intro, baseTitle, baseIntro, error;
        bool loaded = false, pending = false;
        double lastCheckpoint = -10.0;
        TextDraftRecovery::Fields recovered, checkpoint;
        std::set<EntityId> outcomeIds;
        std::string checkpointTitle, checkpointIntro, checkpointBaseTitle, checkpointBaseIntro;
        std::map<EntityId, PublicDraft> checkpointTexts;
    };
    std::map<EntityId, RecapDraft> _recapDrafts;
    std::set<EntityId> _handouts;
    std::shared_ptr<PlayerRecapPackage> _recapPackage;
    std::set<EntityId> _selected, _scenes;
    std::string _message, _nextTitle;
    bool _launcherWasOpen = false;
    bool _presentationWasActive = false, _windowWasFocused = false;
    int _activeTab = -1;
    EntityId _targetSession;
    SessionOutcomes::ApplyReview _applyReview;
    SessionOutcomes::PlayerRecapReview _recapReview;
    SessionOutcomes::CarryForwardRequest _carryRequest;
    bool _carrySubmitted = false;
    std::optional<PreparationTargetRequest> _preparationTarget;
    std::string _preparationTargetTitle;
    std::string _preparationNavigationError;
    std::filesystem::path _preparationRoot;
    std::uint64_t _preparationEpoch = 0;
    bool _preparationOpenEmitted = false;
    void composer(Campaign &, const SessionModel &, const SessionPlan::Persist &, const EntityId &);
    void review(Campaign &, const SessionModel &, const SessionOutcomes::Ledger &, const SessionPlan::Persist &);
    void carry(Campaign &, const SessionModel &, const SessionOutcomes::Ledger &, const SessionPlan::Persist &, const PreparationTargetSink &);
    void drawPreparationTarget(Campaign &, const PreparationTargetSink &);
    void recap(Campaign &, const SessionModel &, const SessionOutcomes::Ledger &, const SessionPlan::Persist &);
    void checkpoint(Campaign &, const EntityId &, Draft &, bool force = false);
    bool prepareRecapDraft(Campaign &, const EntityId &, const SessionOutcomes::Ledger &);
    void checkpointRecap(Campaign &, const EntityId &, bool force = false);
    void pollDrafts(Campaign &, bool force);
    TextDraftRecovery::Fields recapFields(const EntityId &, const RecapDraft &) const;
    bool recapNeedsCheckpoint(const RecapDraft &) const;
    void rememberRecapCheckpoint(RecapDraft &);
    void retainedRecapDrafts(Campaign &, const EntityId &, const SessionOutcomes::Ledger &);
    bool report(const SessionOutcomes::Result &);
};
