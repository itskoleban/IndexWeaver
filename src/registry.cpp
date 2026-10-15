#include "registry.hpp"
#include <stdexcept>
namespace index_weaver {
Registry::Registry(bool bidirectional, std::pmr::memory_resource* memory) : forward_(memory) {
    if (bidirectional) reverse_ = std::make_unique<Map>(memory);
}
Error Registry::set(Value key, Value value, bool insertOnly) {
    if (value < 0) return Error::InvalidArgument;
    auto existing = forward_.find(key);
    if (existing != forward_.end()) {
        if (insertOnly) return Error::DuplicateID;
        if (existing->second == value) return Error::None;
    } else if (size() >= static_cast<std::size_t>(std::numeric_limits<Value>::max())) {
        return Error::Capacity;
    }
    if (reverse_ && reverse_->contains(value)) return Error::OccupiedIndex;
    if (existing != forward_.end()) {
        // Allocate the new reverse entry before changing either existing association.
        if (reverse_) {
            reverse_->emplace(value, key);
            reverse_->erase(existing->second);
        }
        existing->second = value;
    } else {
        auto [entry, inserted] = forward_.emplace(key, value);
        (void)inserted;
        if (reverse_) {
            try { reverse_->emplace(value, key); }
            catch (...) {
                // Roll back then rethrow, never hide unexpected programming failures.
                forward_.erase(entry);
                throw;
            }
        }
    }
    return Error::None;
}
std::optional<Value> Registry::index(Value key) const noexcept {
    const auto it = forward_.find(key);
    return it == forward_.end() ? std::nullopt : std::optional<Value>(it->second);
}
std::optional<Value> Registry::id(Value value) const noexcept {
    if (!reverse_) return std::nullopt;
    const auto it = reverse_->find(value);
    return it == reverse_->end() ? std::nullopt : std::optional<Value>(it->second);
}
bool Registry::remove(Value key) noexcept {
    auto it = forward_.find(key);
    if (it == forward_.end()) return false;
    if (reverse_) reverse_->erase(it->second);
    forward_.erase(it);
    return true;
}
void Registry::clear(bool release) {
    if (release) {
        // Some standard libraries allocate even for an empty map. Construct both
        // replacements before committing either swap, so release is transactional.
        Map empty(forward_.get_allocator().resource());
        if (reverse_) {
            Map back(reverse_->get_allocator().resource());
            forward_.swap(empty); reverse_->swap(back);
        } else forward_.swap(empty);
    } else {
        forward_.clear();
        if (reverse_) reverse_->clear();
    }
}
void Registry::reserve(std::size_t count) {
    if (count > static_cast<std::size_t>(std::numeric_limits<Value>::max()))
        throw std::length_error("capacity exceeds Pawn cell count");
    // On failure bucket counts may change, but associations remain unchanged.
    forward_.reserve(count);
    if (reverse_) reverse_->reserve(count);
}
bool Registry::invariant() const noexcept {
    if (!reverse_) return true;
    if (forward_.size() != reverse_->size()) return false;
    for (const auto& [key, value] : forward_) {
        const auto it = reverse_->find(value);
        if (it == reverse_->end() || it->second != key) return false;
    }
    return true;
}
}
