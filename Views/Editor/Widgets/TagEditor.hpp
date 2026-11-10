#pragma once

#include <string>
#include <vector>

struct TagEditorState
{
    std::string draftTag;
};

bool renderTagEditor(const char *id,
                     std::vector<std::string> &tags,
                     TagEditorState &state,
                     const std::vector<std::string> &suggestions = {});
