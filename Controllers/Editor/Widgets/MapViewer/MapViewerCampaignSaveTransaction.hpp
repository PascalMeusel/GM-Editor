#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <utility>
#include <vector>

// Detail-map and travel acceptance both change several campaign collections
// at once. This small controller-boundary transaction keeps those mutations
// retryable when persistence fails without introducing a second map/domain
// transaction API. The persistence callable is injected so failure behavior
// is deterministic and testable without a database.
template <typename Candidate>
class MapViewerCampaignSaveTransaction
{
public:
    MapViewerCampaignSaveTransaction(
        Campaign &campaign,
        int &selectedMapIndex,
        std::vector<Candidate> &candidates,
        int &selectedCandidateIndex)
        : _campaign(campaign),
          _selectedMapIndex(selectedMapIndex),
          _candidates(candidates),
          _selectedCandidateIndex(selectedCandidateIndex),
          _campaignBefore(campaign),
          _selectedMapIndexBefore(selectedMapIndex),
          _candidatesBefore(candidates),
          _selectedCandidateIndexBefore(selectedCandidateIndex)
    {
    }

    template <typename Persist>
    bool persistOrRollback(Persist persist)
    {
        if (_settled)
            return false;
        try
        {
            if (persist())
            {
                _settled = true;
                return true;
            }
        }
        catch (...)
        {
            rollback();
            throw;
        }
        rollback();
        return false;
    }

    void rollback()
    {
        if (_settled)
            return;
        _campaign = std::move(_campaignBefore);
        _selectedMapIndex = _selectedMapIndexBefore;
        _candidates = std::move(_candidatesBefore);
        _selectedCandidateIndex = _selectedCandidateIndexBefore;
        _settled = true;
    }

private:
    Campaign &_campaign;
    int &_selectedMapIndex;
    std::vector<Candidate> &_candidates;
    int &_selectedCandidateIndex;
    Campaign _campaignBefore;
    int _selectedMapIndexBefore = 0;
    std::vector<Candidate> _candidatesBefore;
    int _selectedCandidateIndexBefore = -1;
    bool _settled = false;
};

