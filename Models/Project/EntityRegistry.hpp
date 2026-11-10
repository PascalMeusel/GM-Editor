#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct EntityRegistryEntry
{
    EntityId id;
    std::string type;
    std::string displayName;
    EntityId campaignId;
    bool exists = true;
};

class EntityRegistry
{
public:
    static EntityRegistry &getInstance();

    void rebuild(const Campaign &campaign);
    const EntityRegistryEntry *find(const EntityId &id) const;
    bool exists(const EntityId &id) const;
    std::string displayNameFor(const EntityId &id) const;
    std::vector<EntityRegistryEntry> entries() const;
    std::size_t size() const { return _entries.size(); }

private:
    void populate(const Campaign &campaign);
    void add(const EntityId &id, const std::string &type, const std::string &displayName, const EntityId &campaignId);

    std::unordered_map<EntityId, EntityRegistryEntry> _entries;
};
