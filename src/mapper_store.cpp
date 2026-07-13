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
	void IndexRegistryStore::debugLog(const char* fmt, ...) noexcept
	{
		std::va_list args;
		va_start(args, fmt);
		std::vfprintf(stderr, fmt, args);
		std::fputc('\n', stderr);
		va_end(args);
	}

	robin_hood::unordered_flat_map<int, int>&
	IndexRegistryStore::getRegistryForWrite(int registryType)
	{
		if (registryType >= 0 && registryType < FAST_REGISTRY_LIMIT)
		{
			return fast_registries_[registryType];
		}

		return slow_registries_[registryType];
	}

	const robin_hood::unordered_flat_map<int, int>*
	IndexRegistryStore::getRegistryForRead(int registryType) const noexcept
	{
		if (registryType >= 0 && registryType < FAST_REGISTRY_LIMIT)
		{
			return &fast_registries_[registryType];
		}

		auto it = slow_registries_.find(registryType);
		if (it != slow_registries_.end())
		{
			return &it->second;
		}

		return nullptr;
	}

	bool IndexRegistryStore::set(int registryType, int id, int index)
	{
		if (registryType < 0 || index < 0)
		{
			if (shouldLogErrors())
			{
				debugLog("[IndexWeaver] SetMapIndex failed: registryType=%d id=%d index=%d",
						 registryType, id, index);
			}

			return false;
		}

		try
		{
			auto& registry = getRegistryForWrite(registryType);
			auto [idIt, idInserted] = registry.try_emplace(id, index);

			if (!idInserted)
			{
				idIt->second = index;

				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] SetMapIndex updated: type=%d id=%d index=%d",
							 registryType, id, index);
				}
			}
			else
			{
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
			if (shouldLogErrors())
			{
				debugLog("[IndexWeaver] SetMapIndex exception: type=%d id=%d", registryType, id);
			}

			return false;
		}
	}

	int IndexRegistryStore::get(int registryType, int id) const noexcept
	{
		const auto* registry = getRegistryForRead(registryType);

		if (!registry)
		{
			return INVALID_INDEX;
		}

		const auto idIt = registry->find(id);
		return (idIt != registry->end()) ? idIt->second : INVALID_INDEX;
	}

	bool IndexRegistryStore::has(int registryType, int id) const noexcept
	{
		const auto* registry = getRegistryForRead(registryType);

		if (!registry)
		{
			return false;
		}

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

		if (registryType >= 0 && registryType < FAST_REGISTRY_LIMIT)
		{
			auto& registry = fast_registries_[registryType];
			if (registry.erase(id) == 0)
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] RemoveMapIndex miss: type=%d id=%d", registryType, id);
				}

				return false;
			}
		}
		else
		{
			auto it = slow_registries_.find(registryType);
			if (it == slow_registries_.end())
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] RemoveMapIndex miss: type=%d id=%d", registryType, id);
				}

				return false;
			}

			if (it->second.erase(id) == 0)
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] RemoveMapIndex miss: type=%d id=%d", registryType, id);
				}

				return false;
			}

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

		if (registryType >= 0 && registryType < FAST_REGISTRY_LIMIT)
		{
			auto& registry = fast_registries_[registryType];
			removed = registry.size();
			if (removed == 0)
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] ClearMapRegistry miss: type=%d", registryType);
				}

				return false;
			}

			registry.clear();
		}
		else
		{
			auto it = slow_registries_.find(registryType);
			if (it == slow_registries_.end())
			{
				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] ClearMapRegistry miss: type=%d", registryType);
				}

				return false;
			}

			removed = it->second.size();
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
		for (auto& reg : fast_registries_)
		{
			reg.clear();
		}

		slow_registries_.clear();

		if (shouldLogVerbose())
		{
			debugLog("[IndexWeaver] ClearAllMapRegistries ok");
		}
	}

	bool IndexRegistryStore::reserve(int registryType, std::size_t capacity)
	{
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
		const auto* registry = getRegistryForRead(registryType);
		return registry ? registry->size() : 0;
	}

	std::size_t IndexRegistryStore::totalRegistries() const noexcept
	{
		std::size_t count = 0;

		for (const auto& pair : slow_registries_)
		{
			if (!pair.second.empty())
			{
				count++;
			}
		}
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