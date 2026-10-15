#pragma once
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <optional>
#include <unordered_map>

namespace index_weaver {
using Value = std::int32_t;
enum class Error : Value { None, InvalidArgument, InvalidHandle, WrongOwner, Missing,
    DuplicateID, OccupiedIndex, NotBidirectional, Allocation, Capacity, HandleExhausted,
    InvalidReference, Internal };
struct Hash {
    std::size_t operator()(Value v) const noexcept {
        auto x = static_cast<std::uint32_t>(v);
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return x;
    }
};
// Main-thread only. No references/iterators are exposed across mutations.
class Registry final {
    using Map = std::pmr::unordered_map<Value, Value, Hash>;
    Map forward_;
    std::unique_ptr<Map> reverse_;
public:
    explicit Registry(bool bidirectional = false,
        std::pmr::memory_resource* memory = std::pmr::get_default_resource());
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Error set(Value id, Value index, bool insertOnly = false);
    std::optional<Value> index(Value id) const noexcept;
    std::optional<Value> id(Value index) const noexcept;
    bool remove(Value id) noexcept;
    void clear(bool release = false);
    void reserve(std::size_t count);
    std::size_t size() const noexcept { return forward_.size(); }
    std::size_t buckets() const noexcept { return forward_.bucket_count(); }
    bool bidirectional() const noexcept { return reverse_ != nullptr; }
    bool invariant() const noexcept;
};
}
