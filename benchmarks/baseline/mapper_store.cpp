/**
 * @file mapper_store.cpp
 * @brief Implementation of the IndexWeaver hybrid storage system.
 *
 * This file contains the implementation of the IndexRegistryStore class, handling
 * the logic for fast array-based registries and dynamic map-based registries,
 * as well as memory retention optimizations.
 */
#include "mapper_store.hpp"

#include <cstdarg>
#include <cstdio>

namespace index_weaver
{
	// Variadic logging function to output formatted strings to the server console.
	void IndexRegistryStore::debugLog(const char* fmt, ...) noexcept
	{
		std::va_list args;
		va_start(args, fmt);

		// Print the formatted string directly to standard error
		std::vfprintf(stderr, fmt, args);

		// Append a newline character for clean console formatting
		std::fputc('\n', stderr);

		va_end(args);
	}

	// Helper method to retrieve the correct map instance for modifications (Insert/Delete).
	robin_hood::unordered_flat_map<int, int>&
	IndexRegistryStore::getRegistryForWrite(int registryType)
	{
		// If the registry category is within the fast limits (0 to 63),
		// access the pre-allocated contiguous std::array. This avoids map lookups.
		if (registryType >= 0 && registryType < FAST_REGISTRY_LIMIT)
		{
			return fast_registries_[registryType];
		}

		// Otherwise, it falls back to the dynamic outer map.
		// Using the subscript operator [] guarantees that if the inner map
		// doesn't exist, it will be default-constructed on the fly.
		return slow_registries_[registryType];
	}

	// Helper method to retrieve a read-only pointer to the map instance.
	// Returns nullptr if the registry does not exist, avoiding accidental creation.
	const robin_hood::unordered_flat_map<int, int>*
	IndexRegistryStore::getRegistryForRead(int registryType) const noexcept
	{
		// Fast path: the std::array elements are always instantiated, so we just return the
		// address.
		if (registryType >= 0 && registryType < FAST_REGISTRY_LIMIT)
		{
			return &fast_registries_[registryType];
		}

		// Slow path: we must search the outer map to see if this dynamic registry was ever created.
		auto it = slow_registries_.find(registryType);

		// If found, return a pointer to the inner map.
		if (it != slow_registries_.end())
		{
			return &it->second;
		}

		// If the registry doesn't exist, return nullptr to indicate a miss.
		return nullptr;
	}

	bool IndexRegistryStore::set(int registryType, int id, int index)
	{
		// Validation guard: negative registry categories and array indices are strictly illegal.
		if (registryType < 0 || index < 0)
		{
			// Only output the error if logging is enabled.
			if (shouldLogErrors())
			{
				debugLog("[IndexWeaver] SetMapIndex failed: registryType=%d id=%d index=%d",
						 registryType, id, index);
			}

			return false; // Exit early to prevent memory corruption
		}

		try
		{
			// Grab a mutable reference to the target map (either fast or slow).
			auto& registry = getRegistryForWrite(registryType);

			// Attempt to insert the ID and Index pair.
			// try_emplace avoids constructing the value if the key already exists.
			auto [idIt, idInserted] = registry.try_emplace(id, index);

			// If the ID was already mapped previously, the insertion failed.
			if (!idInserted)
			{
				// Overwrite the old array index with the new one.
				idIt->second = index;

				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] SetMapIndex updated: type=%d id=%d index=%d",
							 registryType, id, index);
				}
			}
			else
			{
				// Successful fresh insertion.
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] SetMapIndex inserted: type=%d id=%d index=%d",
							 registryType, id, index);
				}
			}

			return true;
		}
		catch (...)
		{
			// Catch any STL allocation exceptions (like std::bad_alloc) to prevent the server from
			// crashing.
			if (shouldLogErrors())
			{
				debugLog("[IndexWeaver] SetMapIndex exception: type=%d id=%d", registryType, id);
			}

			return false;
		}
	}

	int IndexRegistryStore::get(int registryType, int id) const noexcept
	{
		// Try to locate the specific registry (either in the array or the dynamic map).
		const auto* registry = getRegistryForRead(registryType);

		// If the registry category itself doesn't exist, we immediately return the invalid flag.
		if (!registry)
		{
			return INVALID_INDEX;
		}

		// Perform an O(1) hash search for the exact ID within the found registry.
		const auto idIt = registry->find(id);

		// If found, return the array index (`idIt->second`), otherwise return INVALID_INDEX.
		return (idIt != registry->end()) ? idIt->second : INVALID_INDEX;
	}

	bool IndexRegistryStore::has(int registryType, int id) const noexcept
	{
		// Locate the read-only registry.
		const auto* registry = getRegistryForRead(registryType);

		// If the registry doesn't exist, the ID obviously isn't mapped.
		if (!registry)
		{
			return false;
		}

		// Use the modern C++20 contains() method to check for existence efficiently.
		return registry->contains(id);
	}

	bool IndexRegistryStore::remove(int registryType, int id) noexcept
	{
		if (registryType < 0)
		{
			if (shouldLogErrors())
			{
				debugLog("[IndexWeaver] RemoveMapIndex failed: invalid type=%d id=%d", registryType,
						 id);
			}

			return false;
		}

		// Fast Path: Contiguous array of registries
		if (registryType >= 0 && registryType < FAST_REGISTRY_LIMIT)
		{
			// Grab a reference to the fixed inner map.
			auto& registry = fast_registries_[registryType];

			// Attempt to erase the mapping.
			// ZERO-ALLOCATION RETENTION: We do not shrink or destroy the map here.
			// Erasing elements keeps the allocated buckets in memory, making future
			// insertions blazingly fast without triggering OS memory allocations.
			if (registry.erase(id) == 0)
			{
				// Erase returned 0, meaning the ID was not found.
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] RemoveMapIndex miss: type=%d id=%d", registryType, id);
				}

				return false;
			}
		}
		else
		{
			// Slow Path: Dynamic outer hash map.
			auto it = slow_registries_.find(registryType);

			// If the registry category itself doesn't exist, we exit early.
			if (it == slow_registries_.end())
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] RemoveMapIndex miss: type=%d id=%d", registryType, id);
				}

				return false;
			}

			// The registry exists, attempt to erase the mapping from the inner map.
			if (it->second.erase(id) == 0)
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] RemoveMapIndex miss: type=%d id=%d", registryType, id);
				}

				return false;
			}

			// CLEANUP LOGIC: Unlike the fast path, slow registries are unbounded.
			// If a dynamic registry becomes completely empty after this removal,
			// we actively destroy it from the outer map to prevent long-term memory leaks.
			if (it->second.empty())
			{
				slow_registries_.erase(it);
			}
		}

		if (shouldLogVerbose())
		{
			debugLog("[IndexWeaver] RemoveMapIndex ok: type=%d id=%d", registryType, id);
		}

		return true;
	}

	bool IndexRegistryStore::clearType(int registryType) noexcept
	{
		if (registryType < 0)
		{
			if (shouldLogErrors())
			{
				debugLog("[IndexWeaver] ClearMapRegistry failed: invalid type=%d", registryType);
			}

			return false;
		}

		std::size_t removed = 0;

		// Fast Path: Contiguous array of registries
		if (registryType >= 0 && registryType < FAST_REGISTRY_LIMIT)
		{
			auto& registry = fast_registries_[registryType];
			removed = registry.size();

			// If it's already empty, there is nothing to clear.
			if (removed == 0)
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] ClearMapRegistry miss: type=%d", registryType);
				}

				return false;
			}

			// ZERO-ALLOCATION RETENTION: We only clear the elements.
			// The bucket capacity remains allocated in RAM for blazing fast future re-use.
			registry.clear();
		}
		else
		{
			// Slow Path: Dynamic outer hash map.
			auto it = slow_registries_.find(registryType);

			// If the registry category itself doesn't exist, exit early.
			if (it == slow_registries_.end())
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] ClearMapRegistry miss: type=%d", registryType);
				}

				return false;
			}

			removed = it->second.size();

			// CLEANUP LOGIC: Since the entire registry is being cleared,
			// we fully destroy the inner map and erase its node from the outer map
			// to reclaim all associated heap memory.
			slow_registries_.erase(it);
		}

		if (shouldLogVerbose())
		{
			debugLog("[IndexWeaver] ClearMapRegistry ok: type=%d removed=%zu", registryType,
					 removed);
		}

		return true;
	}

	void IndexRegistryStore::clearAll() noexcept
	{
		// Iterate through the fixed fast-path array and clear elements, retaining capacities.
		for (auto& reg : fast_registries_)
		{
			reg.clear();
		}

		// Completely wipe and destroy the slow-path outer map, reclaiming all its memory.
		slow_registries_.clear();

		if (shouldLogVerbose())
		{
			debugLog("[IndexWeaver] ClearAllMapRegistries ok");
		}
	}

	bool IndexRegistryStore::reserve(int registryType, std::size_t capacity)
	{
		// Validation guard: ensure the capacity is reasonable and the registry type is valid.
		if (registryType < 0 || capacity == 0 || capacity > MAX_RESERVED_CAPACITY)
		{
			if (shouldLogErrors())
			{
				debugLog("[IndexWeaver] ReserveMapRegistry failed: type=%d capacity=%zu",
						 registryType, capacity);
			}

			return false;
		}

		try
		{
			// Get the target map (this will auto-create the slow map if it didn't exist)
			// and instruct the robin_hood implementation to pre-allocate memory.
			getRegistryForWrite(registryType).reserve(capacity);

			if (shouldLogVerbose())
			{
				debugLog("[IndexWeaver] ReserveMapRegistry ok: type=%d capacity=%zu", registryType,
						 capacity);
			}

			return true;
		}
		catch (...)
		{
			if (shouldLogErrors())
			{
				debugLog("[IndexWeaver] ReserveMapRegistry exception: type=%d capacity=%zu",
						 registryType, capacity);
			}

			return false;
		}
	}

	std::size_t IndexRegistryStore::count(int registryType) const noexcept
	{
		// Retrieve the read-only registry.
		const auto* registry = getRegistryForRead(registryType);

		// If it exists, return its active element count; otherwise, return 0.
		return registry ? registry->size() : 0;
	}

	std::size_t IndexRegistryStore::totalRegistries() const noexcept
	{
		std::size_t count = 0;

		// Count all dynamic registries that are currently alive in the outer map.
		// (Our cleanup logic guarantees they are never empty, but we check just to be safe).
		for (const auto& pair : slow_registries_)
		{
			if (!pair.second.empty())
			{
				count++;
			}
		}

		// Count all fast-path array registries that contain at least one element.
		for (const auto& reg : fast_registries_)
		{
			if (!reg.empty())
			{
				count++;
			}
		}

		return count;
	}
} // namespace index_weaver