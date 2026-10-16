#pragma once
#include "registry.hpp"
#include <unordered_map>
namespace index_weaver {
using Owner = const void*;
using Handle = Value;
class Manager final {
    struct Entry { Owner owner; std::unique_ptr<Registry> registry; };
    struct Script {
        Error error = Error::None;
        std::unordered_map<Value, std::unique_ptr<Registry>, Hash> legacy;
    };
    std::unordered_map<Handle, Entry, Hash> registries_;
    std::unordered_map<Owner, Script> scripts_;
    // Never reused, including across reset/unload. Exhaustion is explicit.
    std::uint32_t next_ = 1;
public:
    void load(Owner owner);
    void unload(Owner owner) noexcept;
    void reset() noexcept;
    void shutdown() noexcept;
    bool loaded(Owner owner) const noexcept { return scripts_.contains(owner); }
    Handle create(Owner owner, bool reverse, std::size_t reserve);
    Registry* get(Owner owner, Handle handle) noexcept;
    bool destroy(Owner owner, Handle handle) noexcept;
    Registry* legacy(Owner owner, Value category, bool create);
    bool clearLegacy(Owner owner, Value category) noexcept;
    void clearLegacyAll(Owner owner) noexcept;
    std::size_t countLegacy(Owner owner) const noexcept;
    void error(Owner owner, Error error) noexcept;
    Error error(Owner owner) const noexcept;
};
}
