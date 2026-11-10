#pragma once

#include "Models/Map/MapModels.hpp"

#include <utility>

// Scope the existing controller's campaign mutations until the durable save
// succeeds. The editable draft, selections, world links and party location
// return to their pre-attempt values on failure or an exception.
template <typename CampaignState>
class MapCreatorCampaignSaveTransaction
{
public:
    MapCreatorCampaignSaveTransaction(CampaignState &campaign, MapModel &draft,
                                       int &editingIndex, int &selectedIndex)
        : _campaign(campaign), _draft(draft), _editingIndex(editingIndex), _selectedIndex(selectedIndex),
          _previousCampaign(campaign), _previousDraft(draft),
          _previousEditingIndex(editingIndex), _previousSelectedIndex(selectedIndex)
    {
    }
    MapCreatorCampaignSaveTransaction(const MapCreatorCampaignSaveTransaction &) = delete;
    MapCreatorCampaignSaveTransaction &operator=(const MapCreatorCampaignSaveTransaction &) = delete;
    ~MapCreatorCampaignSaveTransaction() { rollback(); }

    bool finish(bool persisted)
    {
        if (persisted)
            _finished = true;
        else
            rollback();
        return persisted;
    }

private:
    void rollback()
    {
        if (_finished)
            return;
        _campaign = std::move(_previousCampaign);
        _draft = std::move(_previousDraft);
        _editingIndex = _previousEditingIndex;
        _selectedIndex = _previousSelectedIndex;
        _finished = true;
    }
    CampaignState &_campaign;
    MapModel &_draft;
    int &_editingIndex;
    int &_selectedIndex;
    CampaignState _previousCampaign;
    MapModel _previousDraft;
    int _previousEditingIndex;
    int _previousSelectedIndex;
    bool _finished = false;
};
