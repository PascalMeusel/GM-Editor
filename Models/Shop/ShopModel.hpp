#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include <string>
#include <vector>

struct ShopStockEntryModel
{
    EntityId itemId;
    int quantity = 1;
    double priceMultiplier = 1.0;
    double sellPriceMultiplier = 0.5;
    bool restocks = false;
    std::string availability = "Common";
    std::string haggleNotes;
    double factionModifier = 1.0;
    double reputationModifier = 1.0;

    ShopStockEntryModel() = default;

    ShopStockEntryModel(const EntityId &itemIdValue, int quantityValue = 1, double priceMultiplierValue = 1.0, bool restocksValue = false)
        : itemId(itemIdValue), quantity(quantityValue), priceMultiplier(priceMultiplierValue), restocks(restocksValue)
    {
    }
};

struct ShopModel
{
    EntityMetadata metadata{"shop"};
    std::string name;
    std::string merchantName;
    std::string description;
    std::string factionReputationNotes;
    EntityId inventoryId;
    std::vector<ShopStockEntryModel> stock;
};
