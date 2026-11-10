#include "WorldInspectorServiceInternal.hpp"

namespace WorldInspector
{
using namespace detail;
MutationResult createShop(Campaign &campaign, const std::string &name)
{
    ShopModel shop;
    shop.name = name.empty() ? "New Shop" : name;
    shop.merchantName = "Merchant";
    const EntityId id = shop.metadata.id;
    campaign.shops.push_back(std::move(shop));
    campaign.metadata.touch();
    return result(MutationStatus::Applied, "Shop created.", id);
}

MutationResult updateShop(Campaign &campaign, const EntityId &shopId, const ShopModel &replacement)
{
    ShopModel *shop = findShop(campaign, shopId);
    if (!shop)
        return result(MutationStatus::NotFound, "The selected shop no longer exists.");
    if (replacement.name.empty())
        return result(MutationStatus::Invalid, "A shop name is required.");
    const EntityMetadata metadata = shop->metadata;
    *shop = replacement;
    shop->metadata = metadata;
    touch(campaign, shop->metadata);
    return result(MutationStatus::Applied, "Shop details saved.", shopId);
}

MutationResult deleteShop(Campaign &campaign, const EntityId &shopId)
{
    auto found = std::find_if(campaign.shops.begin(), campaign.shops.end(), [&](const ShopModel &shop) { return shop.metadata.id == shopId; });
    if (found == campaign.shops.end())
        return result(MutationStatus::NotFound, "The selected shop no longer exists.");
    campaign.shops.erase(found);
    campaign.metadata.touch();
    return result(MutationStatus::Applied, "Shop deleted.", shopId);
}

const ItemModel *findAvailableItem(const Campaign &campaign, const EntityId &itemId)
{
    auto custom = std::find_if(campaign.customItems.begin(), campaign.customItems.end(), [&](const ItemModel &item) { return item.metadata.id == itemId; });
    if (custom != campaign.customItems.end())
        return &*custom;
    if (!campaign.ruleSet)
        return nullptr;
    static thread_local std::vector<ItemModel> rulesetItems;
    rulesetItems = campaign.ruleSet->getItems();
    auto builtin = std::find_if(rulesetItems.begin(), rulesetItems.end(), [&](const ItemModel &item) { return item.metadata.id == itemId; });
    return builtin == rulesetItems.end() ? nullptr : &*builtin;
}

MutationResult addStock(Campaign &campaign, const EntityId &shopId, const EntityId &itemId)
{
    ShopModel *shop = findShop(campaign, shopId);
    if (!shop)
        return result(MutationStatus::NotFound, "The selected shop no longer exists.");
    if (!findAvailableItem(campaign, itemId))
        return result(MutationStatus::NotFound, "The selected item definition is missing.");
    auto found = std::find_if(shop->stock.begin(), shop->stock.end(), [&](const ShopStockEntryModel &stock) { return stock.itemId == itemId; });
    if (found == shop->stock.end())
        shop->stock.emplace_back(itemId, 1);
    else
        ++found->quantity;
    touch(campaign, shop->metadata);
    return result(MutationStatus::Applied, "Shop stock updated.", shopId);
}

MutationResult updateStock(Campaign &campaign, const EntityId &shopId, std::size_t stockIndex, const ShopStockEntryModel &replacement)
{
    ShopModel *shop = findShop(campaign, shopId);
    if (!shop)
        return result(MutationStatus::NotFound, "The selected shop no longer exists.");
    if (stockIndex >= shop->stock.size())
        return result(MutationStatus::NotFound, "The selected stock entry no longer exists.");
    if (replacement.itemId.empty() || replacement.quantity < 0 || replacement.priceMultiplier < 0.0 ||
        replacement.sellPriceMultiplier < 0.0 || replacement.factionModifier < 0.0 || replacement.reputationModifier < 0.0)
        return result(MutationStatus::Invalid, "Stock quantities and price modifiers cannot be negative.");
    shop->stock[stockIndex] = replacement;
    touch(campaign, shop->metadata);
    return result(MutationStatus::Applied, "Shop stock saved.", shopId);
}

MutationResult removeStock(Campaign &campaign, const EntityId &shopId, std::size_t stockIndex)
{
    ShopModel *shop = findShop(campaign, shopId);
    if (!shop || stockIndex >= shop->stock.size())
        return result(MutationStatus::NotFound, "The selected stock entry no longer exists.");
    shop->stock.erase(shop->stock.begin() + static_cast<std::ptrdiff_t>(stockIndex));
    touch(campaign, shop->metadata);
    return result(MutationStatus::Applied, "Stock entry removed.", shopId);
}

int partyQuantity(const Campaign &campaign, const EntityId &itemId)
{
    int total = 0;
    for (const InventoryEntryModel &entry : campaign.partyInventory.entries)
        if (entry.itemId == itemId)
            total += std::max(0, entry.quantity);
    return total;
}

MutationResult transferStockToParty(Campaign &campaign, const EntityId &shopId, std::size_t stockIndex, int quantity)
{
    ShopModel *shop = findShop(campaign, shopId);
    if (!shop || stockIndex >= shop->stock.size())
        return result(MutationStatus::NotFound, "The selected stock entry no longer exists.");
    ShopStockEntryModel &stock = shop->stock[stockIndex];
    const ItemModel *item = findAvailableItem(campaign, stock.itemId);
    if (!item)
        return result(MutationStatus::NotFound, "The item definition is missing, so the transaction was not recorded.");
    if (quantity <= 0 || quantity > stock.quantity)
        return result(MutationStatus::Invalid, "Choose a quantity that is available in shop stock.");
    auto partyEntry = std::find_if(campaign.partyInventory.entries.begin(), campaign.partyInventory.entries.end(), [&](const InventoryEntryModel &entry) {
        return entry.itemId == stock.itemId && (item->stackable || entry.quantity <= 0);
    });
    if (partyEntry == campaign.partyInventory.entries.end())
        campaign.partyInventory.entries.emplace_back(stock.itemId, quantity, false, "Party");
    else
        partyEntry->quantity += quantity;
    stock.quantity -= quantity;
    campaign.partyInventory.metadata.touch();
    touch(campaign, shop->metadata);
    return result(MutationStatus::Applied, "Purchase recorded: item moved from shop stock to Party Inventory.", shopId);
}

MutationResult transferPartyToShop(Campaign &campaign, const EntityId &shopId, const EntityId &itemId, int quantity)
{
    ShopModel *shop = findShop(campaign, shopId);
    if (!shop)
        return result(MutationStatus::NotFound, "The selected shop no longer exists.");
    if (!findAvailableItem(campaign, itemId))
        return result(MutationStatus::NotFound, "The item definition is missing, so the transaction was not recorded.");
    if (quantity <= 0 || quantity > partyQuantity(campaign, itemId))
        return result(MutationStatus::Invalid, "Choose a quantity that is available in Party Inventory.");

    int remaining = quantity;
    for (auto it = campaign.partyInventory.entries.begin(); it != campaign.partyInventory.entries.end() && remaining > 0;)
    {
        if (it->itemId != itemId)
        {
            ++it;
            continue;
        }
        const int moved = std::min(remaining, std::max(0, it->quantity));
        it->quantity -= moved;
        remaining -= moved;
        if (it->quantity <= 0)
            it = campaign.partyInventory.entries.erase(it);
        else
            ++it;
    }
    auto stock = std::find_if(shop->stock.begin(), shop->stock.end(), [&](const ShopStockEntryModel &entry) { return entry.itemId == itemId; });
    if (stock == shop->stock.end())
        shop->stock.emplace_back(itemId, quantity);
    else
        stock->quantity += quantity;
    campaign.partyInventory.metadata.touch();
    touch(campaign, shop->metadata);
    return result(MutationStatus::Applied, "Sale recorded: item moved from Party Inventory to shop stock.", shopId);
}

} // namespace WorldInspector
