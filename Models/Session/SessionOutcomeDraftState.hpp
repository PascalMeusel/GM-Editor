#pragma once
#include "Models/Session/SessionOutcomeModels.hpp"
#include <algorithm>

// Private editor state. None of these drafts enter the player projection.
namespace SessionOutcomeDraftState
{
struct PublicText
{
    std::string text, base;
    bool share = false, baseShare = false;
    bool dirty() const { return text != base || share != baseShare; }
};
enum class Disposition { Editable, Rejected, Missing, Changed };
inline const SessionOutcomes::Outcome *find(const SessionOutcomes::Ledger &ledger, const EntityId &id)
{
    const auto found = std::find_if(ledger.outcomes.begin(), ledger.outcomes.end(),
        [&](const auto &outcome) { return outcome.id == id; });
    return found == ledger.outcomes.end() ? nullptr : &*found;
}
inline Disposition classify(const SessionOutcomes::Ledger &ledger, const EntityId &id, const PublicText &draft)
{
    const auto *outcome = find(ledger, id);
    if (!outcome) return Disposition::Missing;
    if (outcome->state == SessionOutcomes::State::Rejected) return Disposition::Rejected;
    if (draft.dirty() && (draft.base != outcome->publicText || draft.baseShare != outcome->shareWithPlayers))
        return Disposition::Changed;
    return Disposition::Editable;
}
inline bool blocksReview(const SessionOutcomes::Ledger &ledger, const EntityId &id, const PublicText &draft)
{
    const auto *outcome = find(ledger, id);
    return classify(ledger, id, draft) == Disposition::Editable && draft.dirty() &&
        (draft.text != outcome->publicText || draft.share != outcome->shareWithPlayers);
}
inline bool headerConflict(const SessionOutcomes::Ledger &ledger, const std::string &title,
                           const std::string &intro, const std::string &baseTitle, const std::string &baseIntro)
{
    return (title != baseTitle || intro != baseIntro) &&
        (ledger.publicTitle != baseTitle || ledger.publicIntro != baseIntro);
}
inline bool checkpointDue(double now, double lastAttempt, bool force)
{ return force || now - lastAttempt >= 2.0; }
}
