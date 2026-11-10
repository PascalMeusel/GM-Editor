#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace gm::assets
{
std::optional<std::filesystem::path> chooseImage(std::string &error, bool documents = false);
}
