#pragma once

#include <robin_hood.h>
#include <cstddef>

namespace index_weaver
{
    class IndexRegistryStore final
    {
    public:
        static constexpr int INVALID_INDEX = -1;

        bool set(int registryType, int id, int index) noexcept;
        [[nodiscard]] int get(int registryType, int id) const noexcept;
        [[nodiscard]] bool has(int registryType, int id) const noexcept;
        bool remove(int registryType, int id) noexcept;

        bool clearType(int registryType) noexcept;
        void clearAll() noexcept;
        
        bool reserve(int registryType, size_t capacity) noexcept;

        [[nodiscard]] size_t count(int registryType) const noexcept;
        [[nodiscard]] size_t totalRegistries() const noexcept;

    private:
        using Registry = robin_hood::unordered_flat_map<int, int>;
        robin_hood::unordered_flat_map<int, Registry> registry_;
    };
}