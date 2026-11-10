#pragma once

#include "Models/Map/MapEditPatchApply.hpp"

class MapEditHistory
{
public:
    explicit MapEditHistory(std::size_t capacity = 100) : _capacity(std::max<std::size_t>(1, capacity)) {}

    bool apply(MapModel &target, const MapGenerationPatch &patch, const std::string &label)
    {
        if (!canApplyMapGenerationPatchAtomically(target, patch))
            return false;
        MapModel before = target;
        MapModel committed = patch.after;
        push(_undo, Entry{before, label});
        _redo.clear();
        using std::swap;
        swap(target, committed);
        return true;
    }

    void capture(MapModel &target, const MapModel &after, const std::string &label)
    {
        push(_undo, Entry{target, label});
        _redo.clear();
        target = after;
        target.revision += 1;
        target.metadata.touch();
    }

    bool undo(MapModel &target)
    {
        if (_undo.empty())
            return false;
        push(_redo, Entry{target, _undo.back().label});
        target = std::move(_undo.back().map);
        _undo.pop_back();
        return true;
    }

    bool redo(MapModel &target)
    {
        if (_redo.empty())
            return false;
        push(_undo, Entry{target, _redo.back().label});
        target = std::move(_redo.back().map);
        _redo.pop_back();
        return true;
    }

    bool canUndo() const { return !_undo.empty(); }
    bool canRedo() const { return !_redo.empty(); }
    const std::string &undoLabel() const { return _undo.empty() ? emptyLabel() : _undo.back().label; }
    const std::string &redoLabel() const { return _redo.empty() ? emptyLabel() : _redo.back().label; }
    void clear() { _undo.clear(); _redo.clear(); }

private:
    struct Entry
    {
        MapModel map;
        std::string label;
    };

    void push(std::vector<Entry> &stack, Entry entry)
    {
        stack.push_back(std::move(entry));
        if (stack.size() > _capacity)
            stack.erase(stack.begin(), stack.begin() + static_cast<std::ptrdiff_t>(stack.size() - _capacity));
    }

    static const std::string &emptyLabel()
    {
        static const std::string empty;
        return empty;
    }

    std::size_t _capacity;
    std::vector<Entry> _undo;
    std::vector<Entry> _redo;
};
