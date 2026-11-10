#pragma once
#include "Models/Session/SessionOutcomeService.hpp"
#include "Models/Session/PlayerDisplayPublicationAssets.hpp"

// Explicit public projection and owned, reviewed image bytes. No GM data is
// handed to the exporters; selected handouts must already be publicly allowed.
class PlayerRecapPackage
{
public:
    PlayerRecapPackage() = default;
    PlayerRecapPackage(const PlayerRecapPackage &) = delete;
    PlayerRecapPackage &operator=(const PlayerRecapPackage &) = delete;
    PlayerRecapPackage(PlayerRecapPackage &&) = delete;
    PlayerRecapPackage &operator=(PlayerRecapPackage &&) = delete;
    bool review(const Campaign &, const EntityId &sessionId, const std::vector<EntityId> &handoutIds);
    bool current(const Campaign &, bool verifyBytes = false) const;
    bool exportTo(const Campaign &, const std::filesystem::path &parent);
    const SessionOutcomes::PlayerRecapDocument &document() const { return document_; }
    const std::vector<PlayerSafeHandout> &attachments() const { return handouts_; }
    std::filesystem::path imagePath(const PlayerSafeHandout &handout) const;
    FrozenImageAsset image(const PlayerSafeHandout &handout) const;
    const std::string &error() const { return error_; }
    const std::filesystem::path &outputDirectory() const { return output_; }
private:
    SessionOutcomes::PlayerRecapReview recap_;
    SessionOutcomes::PlayerRecapDocument document_;
    std::vector<PlayerSafeHandout> handouts_;
    PlayerDisplayPublicationAssets assets_;
    std::filesystem::path output_;
    std::string error_;
};
