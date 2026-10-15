/**
 * @file mapper_store.hpp
 * @brief Core hybrid storage engine for IndexWeaver.
 *
 * Defines the IndexRegistryStore class which manages the split between
 * ultra-fast array registries and dynamic fallback hash map registries.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <robin_hood.h>

namespace index_weaver
{
	/**
	 * @class IndexRegistryStore
	 * @brief Central storage engine for ID-to-Index mapping.
	 *
	 * Uses a hybrid approach:
	 * - Types 0 to 63 use a contiguous std::array of robin_hood maps (Fast Path).
	 * - Types 64+ use an outer robin_hood map to resolve the inner map (Slow Path).
	 */
	class IndexRegistryStore final
	{
	  public:
		/** @brief Returned when a lookup fails to find a mapping. */
		static constexpr int INVALID_INDEX = -1;
		/** @brief Maximum number of elements that can be reserved at once. */
		static constexpr std::size_t MAX_RESERVED_CAPACITY = 10000000;

		/**
		 * @enum DebugLevel
		 * @brief Defines the verbosity of the internal logger.
		 */
		enum class DebugLevel : std::uint8_t
		{
			Off = 0,	///< Silence all logs
			Errors = 1, ///< Log only critical failures and exceptions
			Verbose = 2 ///< Log every single insertion, removal, and lookup trace
		};

		/**
		 * @brief Inserts or updates a mapping.
		 * @param registryType The category ID of the registry.
		 * @param id The primary key or ID.
		 * @param index The array index to map to.
		 * @return true on success, false on failure (e.g., negative registryType).
		 */
		[[nodiscard]] bool set(int registryType, int id, int index);

		/**
		 * @brief Looks up a mapping.
		 * @param registryType The category ID of the registry.
		 * @param id The primary key or ID to search for.
		 * @return The associated index, or INVALID_INDEX if not found.
		 */
		[[nodiscard]] int get(int registryType, int id) const noexcept;

		/**
		 * @brief Checks if a mapping exists.
		 * @param registryType The category ID of the registry.
		 * @param id The primary key or ID to search for.
		 * @return true if found, false otherwise.
		 */
		[[nodiscard]] bool has(int registryType, int id) const noexcept;

		/**
		 * @brief Removes a mapping from the registry.
		 * @param registryType The category ID of the registry.
		 * @param id The primary key or ID to remove.
		 * @return true if removed, false if it did not exist.
		 */
		[[nodiscard]] bool remove(int registryType, int id) noexcept;

		/**
		 * @brief Wipes all mappings inside a specific registry.
		 * @param registryType The category ID of the registry to clear.
		 * @return true if cleared, false if the registry did not exist.
		 */
		[[nodiscard]] bool clearType(int registryType) noexcept;

		/**
		 * @brief Completely resets and clears all registries.
		 */
		void clearAll() noexcept;

		/**
		 * @brief Pre-allocates memory for a registry to avoid rehashing.
		 * @param registryType The category ID of the registry.
		 * @param capacity The number of elements to reserve space for.
		 * @return true on success, false on invalid parameters.
		 */
		[[nodiscard]] bool reserve(int registryType, std::size_t capacity);

		/**
		 * @brief Gets the number of elements in a specific registry.
		 * @param registryType The category ID of the registry.
		 * @return The element count.
		 */
		[[nodiscard]] std::size_t count(int registryType) const noexcept;

		/**
		 * @brief Gets the total number of non-empty registries currently tracked.
		 * @return The total count of active registries.
		 */
		[[nodiscard]] std::size_t totalRegistries() const noexcept;

		/**
		 * @brief Dynamically adjusts the logger verbosity at runtime.
		 * @param level The desired debug level.
		 */
		void setDebugLevel(DebugLevel level) noexcept
		{
			debug_level_ = level;
		}

		/**
		 * @brief Retrieves the current logger verbosity.
		 * @return The active DebugLevel.
		 */
		[[nodiscard]] DebugLevel debugLevel() const noexcept
		{
			return debug_level_;
		}

	  private:
		/**
		 * @brief Internal printf-style formatted logger.
		 * @param fmt The format string.
		 */
		static void debugLog(const char* fmt, ...) noexcept;

		/** @brief Fast check to see if we should incur logging overhead for errors. */
		[[nodiscard]] bool shouldLogErrors() const noexcept
		{
			return debug_level_ != DebugLevel::Off;
		}

		/** @brief Fast check to see if we should incur logging overhead for traces. */
		[[nodiscard]] bool shouldLogVerbose() const noexcept
		{
			return debug_level_ == DebugLevel::Verbose;
		}

		/** @brief Threshold under which registries use the contiguous array fast path. */
		static constexpr int FAST_REGISTRY_LIMIT = 64;

		/** @brief Fast path: Fixed array for constant O(1) external lookup. */
		std::array<robin_hood::unordered_flat_map<int, int>, FAST_REGISTRY_LIMIT> fast_registries_;

		/** @brief Slow path: Dynamic map for unbounded registry types. */
		robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, int>>
			slow_registries_;

		/** @brief Current debug verbosity level. */
		DebugLevel debug_level_ = DebugLevel::Off;

		/**
		 * @brief Helper to resolve the correct hash map for writing operations.
		 * @param registryType The category ID.
		 * @return A mutable reference to the target robin_hood map.
		 */
		robin_hood::unordered_flat_map<int, int>& getRegistryForWrite(int registryType);

		/**
		 * @brief Helper to resolve the correct hash map for read-only operations.
		 * @param registryType The category ID.
		 * @return A const pointer to the target map, or nullptr if the slow map doesn't exist.
		 */
		const robin_hood::unordered_flat_map<int, int>*
		getRegistryForRead(int registryType) const noexcept;
	};
} // namespace index_weaver