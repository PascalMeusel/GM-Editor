#pragma once
#include "RulesetDefinitionModel.hpp"
#include <memory>

class IRuleSet;
namespace SpellcastingValues
{
enum class Reason {
    None, NoProvider, PinsChanged, ProviderChanged, MissingBase, MissingOverlay,
    AmbiguousBase, AmbiguousOverlay, InvalidDefinition, InvalidFormula
};
}

// Runtime-only evidence of the exact load/pin operation; never persisted.
struct RulesetResolutionState
{
    bool bound = false;
    RulesetReference requested;
    std::weak_ptr<IRuleSet> provider;
    SpellcastingValues::Reason reason = SpellcastingValues::Reason::None;
};
