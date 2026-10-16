#include "manager.hpp"
namespace index_weaver {
void Manager::load(Owner owner) {
    // Repeated load for the same AMX starts a fresh ownership lifetime.
    unload(owner); scripts_.try_emplace(owner);
}
void Manager::unload(Owner owner) noexcept {
    std::erase_if(registries_, [owner](const auto& p) { return p.second.owner == owner; });
    scripts_.erase(owner);
}
void Manager::shutdown() noexcept {
    reset(); scripts_.clear();
}
void Manager::reset() noexcept {
    registries_.clear();
    for (auto& [owner, script] : scripts_) { (void)owner; script.legacy.clear(); script.error = Error::None; }
}
void Manager::error(Owner owner, Error value) noexcept {
    auto it = scripts_.find(owner); if (it != scripts_.end()) it->second.error = value;
}
Error Manager::error(Owner owner) const noexcept {
    const auto it = scripts_.find(owner);
    return it == scripts_.end() ? Error::InvalidHandle : it->second.error;
}
Handle Manager::create(Owner owner, bool reverse, std::size_t capacity) {
    if (!loaded(owner)) return 0;
    if (next_ > static_cast<std::uint32_t>(std::numeric_limits<Value>::max())) {
        error(owner, Error::HandleExhausted); return 0;
    }
    auto registry = std::make_unique<Registry>(reverse);
    if (capacity) registry->reserve(capacity);
    auto handle = static_cast<Handle>(next_);
    registries_.emplace(handle, Entry{owner, std::move(registry)});
    ++next_; error(owner, Error::None); return handle;
}
Registry* Manager::get(Owner owner, Handle handle) noexcept {
    const auto it = registries_.find(handle);
    if (it == registries_.end()) { error(owner, Error::InvalidHandle); return nullptr; }
    if (it->second.owner != owner) { error(owner, Error::WrongOwner); return nullptr; }
    error(owner, Error::None); return it->second.registry.get();
}
bool Manager::destroy(Owner owner, Handle handle) noexcept {
    if (!get(owner, handle)) return false;
    registries_.erase(handle); return true;
}
Registry* Manager::legacy(Owner owner, Value category, bool create) {
    auto s = scripts_.find(owner);
    if (s == scripts_.end()) return nullptr;
    if (category < 0) { error(owner, Error::InvalidArgument); return nullptr; }
    auto it = s->second.legacy.find(category);
    if (it != s->second.legacy.end()) return it->second.get();
    if (!create) { error(owner, Error::Missing); return nullptr; }
    auto registry = std::make_unique<Registry>();
    auto* result = registry.get();
    s->second.legacy.emplace(category, std::move(registry)); return result;
}
bool Manager::clearLegacy(Owner owner, Value category) noexcept {
    auto s = scripts_.find(owner); if (s == scripts_.end()) return false;
    auto it = s->second.legacy.find(category);
    if (it == s->second.legacy.end()) return false;
    s->second.legacy.erase(it); return true;
}
void Manager::clearLegacyAll(Owner owner) noexcept {
    auto it = scripts_.find(owner); if (it != scripts_.end()) it->second.legacy.clear();
}
std::size_t Manager::countLegacy(Owner owner) const noexcept {
    const auto it = scripts_.find(owner); if (it == scripts_.end()) return 0;
    std::size_t count = 0;
    for (const auto& [category, registry] : it->second.legacy) {
        (void)category; count += registry->size() != 0;
    }
    return count;
}
}
