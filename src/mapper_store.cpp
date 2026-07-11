#include "mapper_store.hpp"

namespace index_weaver
{
    bool IndexRegistryStore::set(int registryType, int id, int index) noexcept
    {
        if (registryType < 0 || index < 0)
        {
            return false;
        }

        registry_[registryType][id] = index;
        return true;
    }

    int IndexRegistryStore::get(int registryType, int id) const noexcept
    {
        if (registryType < 0)
        {
            return INVALID_INDEX;
        }

        const auto typeIt = registry_.find(registryType);
        if (typeIt == registry_.end())
        {
            return INVALID_INDEX;
        }

        const auto idIt = typeIt->second.find(id);
        if (idIt == typeIt->second.end())
        {
            return INVALID_INDEX;
        }

        return idIt->second;
    }

    bool IndexRegistryStore::has(int registryType, int id) const noexcept
    {
        if (registryType < 0)
        {
            return false;
        }

        const auto typeIt = registry_.find(registryType);
        if (typeIt == registry_.end())
        {
            return false;
        }

        return typeIt->second.contains(id);
    }

    bool IndexRegistryStore::remove(int registryType, int id) noexcept
    {
        if (registryType < 0)
        {
            return false;
        }

        const auto typeIt = registry_.find(registryType);
        if (typeIt == registry_.end())
        {
            return false;
        }

        auto& bucket = typeIt->second;
        const bool erased = bucket.erase(id) > 0;

        if (bucket.empty())
        {
            registry_.erase(typeIt);
        }

        return erased;
    }

    bool IndexRegistryStore::clearType(int registryType) noexcept
    {
        if (registryType < 0)
        {
            return false;
        }

        return registry_.erase(registryType) > 0;
    }

    void IndexRegistryStore::clearAll() noexcept
    {
        registry_.clear();
    }

    bool IndexRegistryStore::reserve(int registryType, size_t capacity) noexcept
    {
        if (registryType < 0 || capacity == 0 || capacity > 10000000)
        {
            return false;
        }

        registry_[registryType].reserve(capacity);
        return true;
    }

    size_t IndexRegistryStore::count(int registryType) const noexcept
    {
        const auto typeIt = registry_.find(registryType);
        return (typeIt != registry_.end()) ? typeIt->second.size() : 0;
    }

    size_t IndexRegistryStore::totalRegistries() const noexcept
    {
        return registry_.size();
    }
}