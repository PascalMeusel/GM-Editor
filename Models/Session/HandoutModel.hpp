#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <string>

struct HandoutModel
{
    EntityMetadata metadata{"handout"};
    std::string title;
    std::string body;
    std::string filePath;
    std::string gmNotes;
};
