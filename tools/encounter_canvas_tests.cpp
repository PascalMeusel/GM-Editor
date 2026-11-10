#include "Models/Combat/EncounterMapCanvas.hpp"
#include <iostream>
#include <limits>
#include <utility>
int main()
{
    int failures = 0;
    const auto check = [&](bool value, const char *message) { if (!value) { ++failures; std::cerr << message << '\n'; } };
    for (const auto viewport : {std::pair<float,float>{500.f,360.f}, {1000.f,560.f}, {330.f,260.f}})
    {
        const float zoom = encounterCanvasFitZoom(80,80,viewport.first,viewport.second);
        const float cell = 32.0f * zoom;
        check(80 * cell <= viewport.first - 16.0f + .01f && 80 * cell <= viewport.second - 16.0f + .01f,
            "80x80 must fit compact viewport without clipping");
        const float origin = 23.0f;
        const float lastCenter = origin + 79.5f * cell;
        check(static_cast<int>((lastCenter - origin) / cell) == 79, "fit scale must retain edge token hit coordinates");
    }
    const float wide = encounterCanvasFitZoom(80,20,500,300);
    check(80 * 32 * wide <= 484.01f && 20 * 32 * wide <= 284.01f, "wide map aspect fit failed");
    const float previous = .25f, next = .5f, scroll = 50.0f, viewport = 400.0f;
    const float after = encounterCanvasZoomScroll(scroll, viewport, previous, next);
    check(std::abs((scroll + viewport*.5f)/previous - (after + viewport*.5f)/next) < .001f,
          "button zoom must preserve map coordinate at viewport center");
    check(encounterCanvasFitZoom(0,80,500,400)==1.f &&
          encounterCanvasFitZoom(80,80,std::numeric_limits<float>::quiet_NaN(),400)==1.f,
          "invalid fit inputs must remain finite");
    std::cout << "Encounter canvas contracts: " << failures << " failures\n";
    return failures ? 1 : 0;
}
