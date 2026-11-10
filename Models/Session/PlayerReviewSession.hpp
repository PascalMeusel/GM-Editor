#pragma once
#include "Models/Session/PlayerDisplayPublicationAssets.hpp"
#include "Models/Session/PlayerReviewMeasurements.hpp"
#include "Models/Session/PlayerReviewCompileWorker.hpp"
#include <memory>

namespace gm::map_render { class MapRenderCompileMemo; }
enum class PlayerReviewPreparationStatus { Idle, Preparing, ForegroundRequired, Failed };

// Private GM comparison owner, never a publication authorization. A matching
// cheap invalidation key only allows the ordinary authoritative confirm path.
class PlayerReviewSession
{
public:
    PlayerReviewSession();
    explicit PlayerReviewSession(PlayerReviewCompileOptions);
    ~PlayerReviewSession();
    PlayerReviewSession(const PlayerReviewSession&)=delete;
    PlayerReviewSession &operator=(const PlayerReviewSession&)=delete;
    bool prepare(const Campaign &,const std::string &locale);
    // A request immediately revokes consent but retains the previous preview.
    // True means accepted, including an immediate explicit-foreground route.
    // Poll adopts only exact current cache hits and never compiles on a miss.
    bool requestPreparation(const Campaign &,const std::string &,std::uint64_t contextEpoch);
    bool pollPreparation(const Campaign &,const std::string &,std::uint64_t contextEpoch);
    bool prepareInForeground(const Campaign &,const std::string &,std::uint64_t contextEpoch);
    void cancelPreparation();
    bool preparing() const;
    bool foregroundRequired() const;
    PlayerReviewPreparationStatus preparationStatus() const;
    std::uint64_t preparationSerial() const;
    PlayerReviewCompileStats compileStats() const;
    PlayerReviewAcceptedCompileStats acceptedCompileStats() const;
    void clear();
    // Authoritative rejection retains the earlier preview but revokes consent.
    void invalidate();
    bool knownCurrent(const Campaign &,const std::string &locale);
    bool prepared() const;
    bool stale() const { return stale_; }
    std::uint64_t generation() const { return generation_; }
    const PlayerDisplayReviewSnapshot &review() const;
    const LocalPlayerDisplayModel &display() const;
    const MapModel *gmMap() const;
    const MapModel *playerMap() const;
    FrozenImageAsset gmBackground() const;
    FrozenImageAsset playerImage(const std::string &path,const std::string &hash) const;
    bool gmArtOmitted() const;
    const std::string &error() const { return error_; }
    void setMeasurements(PlayerDisplayReview::Measurements *m) { measurements_=m; }
    PlayerDisplayReview::Measurements *measurements() const { return measurements_; }
    // Hold only for a confirmation operation. Cache ownership keeps geometry
    // alive across save callbacks; it never keeps review consent alive.
    std::shared_ptr<gm::map_render::MapRenderCompileMemo> compileMemo() const;
private:
    friend struct PlayerReviewSessionTestProbe;
    struct State;
    struct Preparation;
    bool finishPreparedState(const Campaign &,const std::string &,std::unique_ptr<State>,bool advanceGeneration);
    bool preparationMatches(const Campaign &,const std::string &,std::uint64_t) const;
    void rejectPreparation(const std::string &);
    std::unique_ptr<State> state_;
    PlayerReviewCompileOptions compileOptions_;
    std::unique_ptr<Preparation> preparation_;
    bool stale_=false;
    bool preparationVeto_=false;
    std::uint64_t generation_=0;
    std::string error_;
    PlayerDisplayReview::Measurements *measurements_=nullptr;
};
