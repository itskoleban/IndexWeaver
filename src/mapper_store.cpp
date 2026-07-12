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

		const uint64_t key = make_key(registryType, id);

		try
		{
			auto [regIt, regInserted] = registry_.try_emplace(key, index);

			if (!regInserted)
			{
				regIt->second = index;

				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] SetMapIndex updated: type=%d id=%d index=%d",
							 registryType, id, index);
				}
				return true;
			}

			try
			{
				auto [typeIt, typeInserted] = keys_by_type_.try_emplace(registryType);
				(void)typeInserted;

				auto [keyIt, keyInserted] = typeIt->second.insert(key);
				(void)keyIt;

				if (!keyInserted)
				{
					registry_.erase(regIt);
					if (typeIt->second.empty())
					{
						keys_by_type_.erase(typeIt);
					}

					if (shouldLogErrors())
					{
						debugLog("[IndexWeaver] SetMapIndex rollback failed: type=%d id=%d",
								 registryType, id);
					}
					return false;
				}

				if (shouldLogVerbose())
				{
					debugLog("[IndexWeaver] SetMapIndex inserted: type=%d id=%d index=%d",
							 registryType, id, index);
				}
				return true;
			}
			catch (...)
			{
				registry_.erase(regIt);
				if (shouldLogErrors())
				{
					debugLog("[IndexWeaver] SetMapIndex exception: type=%d id=%d", registryType,
							 id);
				}
				return false;
			}
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
		if (registryType < 0)
		{
			return INVALID_INDEX;
		}

		const auto it = registry_.find(make_key(registryType, id));
		return (it != registry_.end()) ? it->second : INVALID_INDEX;
	}

	bool IndexRegistryStore::has(int registryType, int id) const noexcept
	{
		if (registryType < 0)
		{
			return false;
		}

		return registry_.contains(make_key(registryType, id));
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

		const uint64_t key = make_key(registryType, id);

		auto regIt = registry_.find(key);
		if (regIt == registry_.end())
		{
			if (shouldLogVerbose())
			{
				debugLog("[IndexWeaver] RemoveMapIndex miss: type=%d id=%d", registryType, id);
			}
			return false;
		}

		registry_.erase(regIt);

		auto typeIt = keys_by_type_.find(registryType);
		if (typeIt != keys_by_type_.end())
		{
			typeIt->second.erase(key);
			if (typeIt->second.empty())
			{
				keys_by_type_.erase(typeIt);
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

		auto typeIt = keys_by_type_.find(registryType);
		if (typeIt == keys_by_type_.end())
		{
			if (shouldLogVerbose())
			{
				debugLog("[IndexWeaver] ClearMapRegistry miss: type=%d", registryType);
			}
			return false;
		}

		for (uint64_t key : typeIt->second)
		{
			registry_.erase(key);
		}

		const std::size_t removed = typeIt->second.size();
		keys_by_type_.erase(typeIt);

		if (shouldLogVerbose())
		{
			debugLog("[IndexWeaver] ClearMapRegistry ok: type=%d removed=%zu", registryType,
					 removed);
		}
		return true;
	}

	void IndexRegistryStore::clearAll() noexcept
	{
		registry_.clear();
		keys_by_type_.clear();

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
			registry_.reserve(registry_.size() + capacity);

			auto typeIt = keys_by_type_.find(registryType);
			if (typeIt != keys_by_type_.end())
			{
				typeIt->second.reserve(typeIt->second.size() + capacity);
			}

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
		const auto it = keys_by_type_.find(registryType);
		return (it != keys_by_type_.end()) ? it->second.size() : 0;
	}

	std::size_t IndexRegistryStore::totalRegistries() const noexcept
	{
		return keys_by_type_.size();
	}
} // namespace index_weaver