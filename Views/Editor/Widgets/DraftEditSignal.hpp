#pragma once
#include <functional>
#include <utility>

namespace DraftEditSignal
{
inline thread_local const std::function<void()> *active = nullptr;

// Only a draft editor installs a listener. Browsing and ordinary forms retain
// their existing behavior, and nested panels restore the enclosing listener.
class Scope
{
    std::function<void()> listener_;
    const std::function<void()> *previous_;
public:
    explicit Scope(std::function<void()> listener) : listener_(std::move(listener)), previous_(active) { active = &listener_; }
    ~Scope() { active = previous_; }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
};
inline bool record(bool changed)
{
    if (changed && active) (*active)();
    return changed;
}
}
