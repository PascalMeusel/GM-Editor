#pragma once
#include "Models/Session/PlayerReviewMeasurements.hpp"
#include "Views/Editor/Widgets/ImageTextureMeasurements.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace player_perf
{
using Clock = std::chrono::steady_clock;
inline double milliseconds(Clock::duration value) { return std::chrono::duration<double, std::milli>(value).count(); }
inline void require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
inline std::string json(const std::string &value)
{
    std::ostringstream out; out << '"';
    for (unsigned char c : value)
    {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}
struct Frame
{
    std::uint64_t number = 0; int cycle = 0; std::string phase;
    double wall = 0, beforeToAfterRender = 0;
    std::size_t scopedEntries = 0, scopedLoaded = 0;
    gm::image_texture::Measurements imageStages;
};
struct Operation { std::string name; int cycle; double elapsed; };
struct ButtonObservation
{
    std::string action,label; std::uint64_t id=0,inputFrame=0,responseFrame=0;
    double releaseToResponse=0,inputFrameRender=0; bool applied=false;
    PlayerDisplayReview::Measurements beforeReviewStages,afterReviewStages;
};
struct PixelObservation
{
    std::string action; std::uint64_t inputFrame=0,pixelFrame=0;
    double releaseToPixels=0,firstFrameRender=0;
};
struct Metrics
{
    PlayerDisplayReview::Measurements reviewStages;
    gm::image_texture::Measurements imageStages;
    std::vector<Frame> frames;
    std::vector<Operation> operations;
    std::vector<ButtonObservation> buttons;
    std::vector<PixelObservation> pixels;
    std::map<std::string, std::string> facts;
    std::vector<std::string> checks;
    template<class Function> void time(const std::string &name, int cycle, Function action)
    {
        const auto start = Clock::now();
        try { action(); }
        catch (...) { operations.push_back({name + "_failed", cycle, milliseconds(Clock::now() - start)}); throw; }
        operations.push_back({name, cycle, milliseconds(Clock::now() - start)});
    }
    static double percentile(std::vector<double> values, double quantile)
    {
        if (values.empty()) return 0;
        std::sort(values.begin(), values.end());
        return values[std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(values.size() * quantile))) - 1];
    }
    std::vector<double> steady(int cycle = -1) const
    {
        std::vector<double> result;
        for (const auto &f : frames) if (f.phase == "steady" && (cycle < 0 || f.cycle == cycle)) result.push_back(f.wall);
        return result;
    }
    bool performancePassed() const
    {
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            const auto values = steady(cycle);
            if (values.size() < 300 || percentile(values, .95) > 33 || percentile(values, 1) > 100) return false;
        }
        return true;
    }
    void write(const std::filesystem::path &root, bool complete, const std::string &failure) const
    {
        std::ofstream csv(root / "frames.csv"); csv << "frame,cycle,phase,frame_cycle_wall_ms,before_to_after_render_ms,scoped_entries_after_collection,scoped_loaded_after_collection\n";
        for (const auto &f : frames) csv << f.number << ',' << f.cycle << ',' << f.phase << ',' << f.wall << ',' << f.beforeToAfterRender << ',' << f.scopedEntries << ',' << f.scopedLoaded << '\n';
        std::ofstream op(root / "operations.csv"); op << "operation,cycle,elapsed_ms\n";
        for (const auto &o : operations) op << o.name << ',' << o.cycle << ',' << o.elapsed << '\n';
        std::ofstream buttonsCsv(root / "button-controls.csv");
        buttonsCsv<<"action,label,control_id,release_input_frame,response_frame,release_to_response_ms,input_frame_before_to_after_render_ms,applied\n";
        for(const auto &b:buttons)buttonsCsv<<b.action<<','<<b.label<<','<<b.id<<','<<b.inputFrame<<','<<b.responseFrame<<','
            <<b.releaseToResponse<<','<<b.inputFrameRender<<','<<(b.applied?1:0)<<'\n';
        std::ofstream pixelsCsv(root / "button-player-pixels.csv");
        pixelsCsv<<"action,release_input_frame,first_verified_pixel_frame,release_to_pixel_frame_ms,first_pixel_frame_before_to_after_render_ms\n";
        for(const auto &p:pixels)pixelsCsv<<p.action<<','<<p.inputFrame<<','<<p.pixelFrame<<','<<p.releaseToPixels<<','<<p.firstFrameRender<<'\n';
        std::ofstream buttonStages(root / "button-review-stages.csv");
        buttonStages<<"action,release_input_frame,response_frame,origin,stage,before_count,after_count,delta_count,delta_ms\n";
        for(const auto &button:buttons)if(button.applied)
            for(std::size_t origin=0;origin<button.beforeReviewStages.values.size();++origin)
                for(std::size_t stage=0;stage<button.beforeReviewStages.values[origin].size();++stage){
                    const auto &before=button.beforeReviewStages.values[origin][stage],&after=button.afterReviewStages.values[origin][stage];
                    require(after.count>=before.count&&after.milliseconds>=before.milliseconds,"Button review timing counters went backwards");
                    buttonStages<<button.action<<','<<button.inputFrame<<','<<button.responseFrame<<','
                        <<PlayerDisplayReview::originName(PlayerDisplayReview::Origin(origin))<<','<<PlayerDisplayReview::stageName(PlayerDisplayReview::Stage(stage))
                        <<','<<before.count<<','<<after.count<<','<<after.count-before.count<<','<<after.milliseconds-before.milliseconds<<'\n';}
        std::ofstream stages(root / "review-stages.csv");stages<<"origin,stage,count,total_ms\n";
        for(std::size_t origin=0;origin<reviewStages.values.size();++origin)
            for(std::size_t stage=0;stage<reviewStages.values[origin].size();++stage){const auto &value=reviewStages.values[origin][stage];
                stages<<PlayerDisplayReview::originName(PlayerDisplayReview::Origin(origin))<<','
                    <<PlayerDisplayReview::stageName(PlayerDisplayReview::Stage(stage))<<','<<value.count<<','<<value.milliseconds<<'\n';}
        std::ofstream images(root / "image-texture-stages.csv"); images << "origin,stage,count,total_ms\n";
        std::ofstream imageFrames(root / "image-texture-frames.csv"); imageFrames << "frame,cycle,phase,origin,stage,count,total_ms\n";
        for(std::size_t origin=0;origin<imageStages.values.size();++origin)
            for(std::size_t stage=0;stage<imageStages.values[origin].size();++stage)
            {
                const auto &value=imageStages.values[origin][stage];
                const auto *originText=gm::image_texture::originName(gm::image_texture::Origin(origin));
                const auto *stageText=gm::image_texture::stageName(gm::image_texture::Stage(stage));
                images<<originText<<','<<stageText<<','<<value.count<<','<<value.milliseconds<<'\n';
                for(const auto &frame:frames)
                {
                    const auto &sample=frame.imageStages.values[origin][stage];
                    imageFrames<<frame.number<<','<<frame.cycle<<','<<frame.phase<<','<<originText<<','<<stageText
                        <<','<<sample.count<<','<<sample.milliseconds<<'\n';
                }
            }
        std::ofstream out(root / "summary.json");
        out << "{\n\"correctness_complete\":" << (complete ? "true" : "false") << ",\n\"steady_gates_passed\":"
            << (complete && performancePassed() ? "true" : "false") << ",\n\"failure\":" << json(failure)
            << ",\n\"cold_costs_accepted\":false,\n\"recap_scope\":\"real GL cache-owner probe; recap UI not rendered\",\n\"cycles\":[";
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            const auto v = steady(cycle); if (cycle) out << ',';
            out << "{\"cycle\":" << cycle << ",\"count\":" << v.size() << ",\"p50_ms\":" << percentile(v,.5)
                << ",\"p95_ms\":" << percentile(v,.95) << ",\"p99_ms\":" << percentile(v,.99) << ",\"max_ms\":" << percentile(v,1)
                << ",\"over100ms\":" << std::count_if(v.begin(), v.end(), [](double x) { return x > 100; }) << '}';
        }
        out << "],\n\"popup_idle_timings\":[";
        bool firstPopup=true;
        for(const char *phase:{"editor_popup_map_idle","editor_popup_image_idle","legacy_popup_map_idle","legacy_popup_image_idle"}) {
            std::vector<double> values;for(const auto &frame:frames)if(frame.phase==phase)values.push_back(frame.wall);
            if(!firstPopup)out<<',';firstPopup=false;
            out<<"{\"phase\":"<<json(phase)<<",\"count\":"<<values.size()<<",\"p50_ms\":"<<percentile(values,.5)
                <<",\"p95_ms\":"<<percentile(values,.95)<<",\"max_ms\":"<<percentile(values,1)
                <<",\"over100ms\":"<<std::count_if(values.begin(),values.end(),[](double value){return value>100;})<<",\"accepted\":false}";
        }
        out << "],\n\"nonsteady_frame_count_over100ms\":" << std::count_if(frames.begin(),frames.end(),[](const Frame &f) {
            return f.phase!="steady" && f.wall>100;
        }) << ",\n\"cold_control_operations_over100ms\":[";
        bool first=true;
        for(const auto &o:operations) if(o.elapsed>100)
        {
            if(!first) out << ','; first=false;
            out << "{\"operation\":" << json(o.name) << ",\"cycle\":" << o.cycle << ",\"elapsed_ms\":" << o.elapsed << '}';
        }
        out << "],\n\"facts\":{"; first = true;
        for (const auto &[key,value] : facts) { if (!first) out << ','; first=false; out << '\n' << json(key) << ':' << json(value); }
        out << "\n},\n\"completed_checks\":["; first=true;
        for (const auto &check : checks) { if (!first) out << ','; first=false; out << json(check); }
        out << "]\n}\n";
        require(bool(csv) && bool(op) && bool(out) && bool(stages) && bool(images) && bool(imageFrames) &&
            bool(buttonsCsv) && bool(pixelsCsv) && bool(buttonStages), "Could not write performance evidence");
    }
};
}
