#pragma once

#include <string>
#include <vector>

namespace gm::ux
{
enum class VisualLocale
{
    English,
    German
};

struct VisualViewport
{
    int width = 1280;
    int height = 720;
    float uiScale = 1.0f;
    std::string key;
};

struct VisualScenario
{
    std::string id;
    std::string surface;
    std::string seedProfile;
    std::string state;
    bool playerSafe = false;
};

struct VisualCaptureCase
{
    std::string id;
    VisualScenario scenario;
    VisualViewport viewport;
    VisualLocale locale = VisualLocale::English;
};

const std::vector<VisualViewport> &visualViewports();
const std::vector<VisualScenario> &visualScenarios();
std::vector<VisualCaptureCase> visualCaptureMatrix();
const char *visualLocaleCode(VisualLocale locale);
}
