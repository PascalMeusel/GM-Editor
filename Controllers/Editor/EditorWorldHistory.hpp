#pragma once
#include "Models/Project/WorldInspectorHistory.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

class EditorWindowController;
class EditorWorldHistory : public std::enable_shared_from_this<EditorWorldHistory>
{
public:
    explicit EditorWorldHistory(EditorWindowController &owner) : owner_(&owner) {}
    WorldInspector::MutationResult save(WorldInspectorDrafts::Scope,const std::string &reason);
    bool hasPendingRegistration() const noexcept { return static_cast<bool>(pending_); }
    bool canRetryRegistration() const;
    const std::string &registrationError() const noexcept { return error_; }
    const std::string &pendingLabel() const noexcept { return label_; }
    bool retryRegistration();
    bool beforeHistoryAction();
    void reset();
    void detach() noexcept;
private:
    struct Context
    {
        std::uint64_t generation=0;
        EntityId campaignId;
        std::filesystem::path root;
    };
    Context context() const;
    bool current(const Context &) const;
    void perform(const WorldInspectorHistory::Patch &,bool forward,const Context &);
    bool registerPending();
    bool committed(const Context &);
    void revokeCommitted(const Context &);
    bool acknowledge(const WorldInspectorDrafts::Store &,const WorldInspectorDrafts::Scope &);
    EditorWindowController *owner_=nullptr;
    std::uint64_t generation_=0;
    bool busy_=false;
    Context pendingContext_;
    std::shared_ptr<const WorldInspectorHistory::Patch> pending_;
    std::string error_,label_;
};
