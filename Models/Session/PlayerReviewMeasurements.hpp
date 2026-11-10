#pragma once
#include <array>
#include <chrono>
#include <cstdint>

namespace PlayerDisplayReview
{
enum class BuildPurpose { Comparison, PlayerOnly };
enum class Origin { Unspecified, Open, PreSave, PostSave, Installation, Runtime, PreparationWorker, Count };
enum class Stage { Build, GmCompile, PlayerSource, PlayerCompile, HandoutBinding,
    PreviewFilter, PreviewAssets, Invalidation, PreValidation, CampaignCopy, Persist,
    PostValidation, Commit, Installation, AssetCapture, CacheRefresh, ApprovedCopy,
    SourceKeys, ConfirmationOwnership, ConfirmationTotal, UiConfirmationTotal, Count };
inline const char *originName(Origin origin)
{constexpr const char *names[]={"unspecified","open","pre_save","post_save","installation","runtime","preparation_worker"};return names[std::size_t(origin)];}
inline const char *stageName(Stage stage)
{constexpr const char *names[]={"build_inclusive","gm_compile","player_source","player_compile","handout_binding",
    "preview_filter","preview_assets","invalidation","pre_validation_inclusive","campaign_copy","persist_inclusive",
    "post_validation_inclusive","commit","installation_inclusive","asset_capture","cache_refresh_inclusive","approved_copy","source_keys",
    "confirmation_ownership","confirmation_total_inclusive","ui_confirmation_total_inclusive"};return names[std::size_t(stage)];}
struct Measurements
{
    struct Sample { std::uint64_t count=0; double milliseconds=0; };
    std::array<std::array<Sample,static_cast<std::size_t>(Stage::Count)>,static_cast<std::size_t>(Origin::Count)> values{};
};
// Optional caller-owned instrumentation. No clock, allocation or global state
// is touched when no observer is supplied. Inclusive stages must not be summed.
class Measure
{
    Measurements::Sample *sample_=nullptr;
    std::chrono::steady_clock::time_point start_{};
public:
    Measure(Measurements *m,Stage stage,Origin origin=Origin::Unspecified)
    {
        if(m){sample_=&m->values[static_cast<std::size_t>(origin)][static_cast<std::size_t>(stage)];start_=std::chrono::steady_clock::now();}
    }
    ~Measure(){if(sample_){++sample_->count;sample_->milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start_).count();}}
    Measure(const Measure&)=delete;
};
}
