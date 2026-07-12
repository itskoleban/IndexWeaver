#pragma once

#include <cstddef>
#include <cstdint>
#include <robin_hood.h>

namespace index_weaver
{
	class IndexRegistryStore final
	{
	  public:
		static constexpr int INVALID_INDEX = -1;
		static constexpr std::size_t MAX_RESERVED_CAPACITY = 10000000;

		enum class DebugLevel : std::uint8_t
		{
			Off = 0,
			Errors = 1,
			Verbose = 2
		};

		[[nodiscard]] bool set(int registryType, int id, int index);
		[[nodiscard]] int get(int registryType, int id) const noexcept;
		[[nodiscard]] bool has(int registryType, int id) const noexcept;
		[[nodiscard]] bool remove(int registryType, int id) noexcept;

		[[nodiscard]] bool clearType(int registryType) noexcept;
		void clearAll() noexcept;

		[[nodiscard]] bool reserve(int registryType, std::size_t capacity);

		[[nodiscard]] std::size_t count(int registryType) const noexcept;
		[[nodiscard]] std::size_t totalRegistries() const noexcept;

		void setDebugLevel(DebugLevel level) noexcept
		{
			debug_level_ = level;
		}
		[[nodiscard]] DebugLevel debugLevel() const noexcept
		{
			return debug_level_;
		}

	  private:
		static constexpr uint64_t make_key(int type, int id) noexcept
		{
			return (static_cast<uint64_t>(static_cast<uint32_t>(type)) << 32) |
				   static_cast<uint32_t>(id);
		}

		static void debugLog(const char* fmt, ...) noexcept;

		[[nodiscard]] bool shouldLogErrors() const noexcept
		{
			return debug_level_ != DebugLevel::Off;
		}

		[[nodiscard]] bool shouldLogVerbose() const noexcept
		{
			return debug_level_ == DebugLevel::Verbose;
		}

		robin_hood::unordered_flat_map<uint64_t, int> registry_;
		robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<uint64_t>> keys_by_type_;
		DebugLevel debug_level_ = DebugLevel::Off;
	};
} // namespace index_weaver