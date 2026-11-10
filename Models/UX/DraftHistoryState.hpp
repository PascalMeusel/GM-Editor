#pragma once
#include <cstdint>

// Render revisions advance on every mutation, including Undo. Content identity
// instead follows the undo graph, so returning to a saved node can be clean.
class DraftHistoryState
{
public:
    std::uint64_t current() const { return _current; }
    bool dirty() const { return _current != _saved; }
    void edited() { _current = ++_next; }
    void saved() { _saved = _current; }
    void restore(std::uint64_t id) { _current = id; }
private:
    std::uint64_t _next = 0, _current = 0, _saved = 0;
};
