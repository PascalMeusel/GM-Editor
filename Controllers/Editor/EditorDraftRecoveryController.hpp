#pragma once
#include "Models/Project/EditorDraftRecovery.hpp"
#include <chrono>
#include <filesystem>

class ItemCreatorController;
class CharacterSheetController;
class EditorDraftRecoveryController
{
public:
    using Clock=std::chrono::steady_clock;
    EditorDraftRecoveryController(Campaign &,ItemCreatorController &,CharacterSheetController &);
    ~EditorDraftRecoveryController();
    void initialize();
    bool bound() const;
    bool ready();
    bool editable() { return !privateMode() && ready(); }
    bool ownsError(const std::string &value) const { return !_error.empty() && value==_error; }
    bool checkpoint(bool force=true,Clock::time_point now=Clock::now());
    bool restore();
    bool discard(bool savedCheckpoint=false);
    bool retryRead();
    std::string copy();
    bool attention() const;
    bool pending() const { return _pending; }
    bool cleanupPending() const { return _cleanup; }
    bool privateMode() const { return _campaign.presentationModeActive; }
    const std::string &error() const;
    const EditorDraftRecovery::ReadResult &recovered() const { return _read; }
    EditorDraftRecovery::Store drafts() const;
    bool open=false;
private:
    Campaign &_campaign;
    ItemCreatorController &_items;
    CharacterSheetController &_characters;
    EntityId _campaignId;
    std::filesystem::path _root;
    bool _initialized=false,_pending=false,_dirty=false,_cleanup=false;
    Clock::time_point _attempted=Clock::now();
    std::string _lastPayload,_error;
    EditorDraftRecovery::ReadResult _read;
    void committed();
    bool discardNotes();
};
