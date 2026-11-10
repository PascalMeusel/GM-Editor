#pragma once
#include "Models/Campaign/Campaign.hpp"
#include <functional>

namespace SelectedPlayerPreview
{
void npc(const Campaign &, const EntityId &, const std::function<void(EntityId)> &review);
void item(const Campaign &, const EntityId &, const std::function<void(EntityId)> &review,
          const ItemModel *privateDraft = nullptr);
}
