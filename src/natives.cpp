/**
 * @file natives.cpp
 * @brief Implementation of the open.mp Pawn API natives.
 *
 * This file maps the C++ AMX natives exported to Pawn into the internal
 * methods of the global IndexRegistryStore.
 */

#include "natives.hpp"
#include "mapper_store.hpp"

#include <Server/Components/Pawn/Impl/pawn_natives.hpp>

/**
 * @brief Registers or updates an ID-to-Index mapping.
 *
 * Maps a generic identifier (e.g. database ID) to a specific array index.
 * If the mapping already exists, the index is safely overwritten.
 *
 * @param registryType The category ID to store this mapping under.
 * @param id The unique identifier to be mapped.
 * @param index The actual array index where the object is stored.
 * @return true if successful, false if parameters are invalid.
 */
SCRIPT_API(SetMapIndex, bool(int registryType, int id, int index))
{
	return gIndexWeaverStore ? gIndexWeaverStore->set(registryType, id, index) : false;
}

/**
 * @brief Retrieves the array index associated with a specific ID.
 *
 * Performs an O(1) lookup in the registry to find the index.
 *
 * @param registryType The category ID to search within.
 * @param id The unique identifier to look up.
 * @return The mapped index, or INVALID_INDEX (-1) if not found.
 */
SCRIPT_API(GetMapIndex, int(int registryType, int id))
{
	return gIndexWeaverStore ? gIndexWeaverStore->get(registryType, id)
							 : index_weaver::IndexRegistryStore::INVALID_INDEX;
}

/**
 * @brief Removes a specific mapping from a registry.
 *
 * Safely erases the ID from the registry without leaking memory.
 *
 * @param registryType The category ID to remove from.
 * @param id The unique identifier to erase.
 * @return true if the ID existed and was removed, false otherwise.
 */
SCRIPT_API(RemoveMapIndex, bool(int registryType, int id))
{
	return gIndexWeaverStore ? gIndexWeaverStore->remove(registryType, id) : false;
}

/**
 * @brief Checks if a specific ID is currently mapped.
 *
 * @param registryType The category ID to search within.
 * @param id The unique identifier to check.
 * @return true if the ID is mapped, false otherwise.
 */
SCRIPT_API(HasMapIndex, bool(int registryType, int id))
{
	return gIndexWeaverStore ? gIndexWeaverStore->has(registryType, id) : false;
}

/**
 * @brief Wipes all mappings within a specific registry category.
 *
 * Completely clears a registry while leaving other registries untouched.
 *
 * @param registryType The category ID to wipe.
 * @return true if the registry existed and was cleared, false otherwise.
 */
SCRIPT_API(ClearMapRegistry, bool(int registryType))
{
	return gIndexWeaverStore ? gIndexWeaverStore->clearType(registryType) : false;
}

/**
 * @brief Performs a hard reset on the entire storage engine.
 *
 * Wipes every single registry and mapping currently stored in memory.
 */
SCRIPT_API(ClearAllMapRegistries, void())
{
	if (gIndexWeaverStore)
	{
		gIndexWeaverStore->clearAll();
	}
}

/**
 * @brief Pre-allocates memory for a specific registry.
 *
 * Optimizes memory usage and prevents rehashing when bulk-inserting
 * thousands of elements (e.g., loading database entries on boot).
 *
 * @param registryType The category ID to pre-allocate.
 * @param capacity The number of elements to reserve space for.
 * @return true if successful, false on invalid parameters or failure.
 */
SCRIPT_API(ReserveMapRegistry, bool(int registryType, int capacity))
{
	return (gIndexWeaverStore && capacity > 0)
			   ? gIndexWeaverStore->reserve(registryType, static_cast<std::size_t>(capacity))
			   : false;
}

/**
 * @brief Sets the internal verbosity level of the component's logger.
 *
 * Useful for debugging mapping failures or observing the internal behavior.
 * 0 = Off, 1 = Errors Only, 2 = Verbose (Logs everything).
 *
 * @param level The debug level to apply.
 * @return true on success, false if the level is out of bounds.
 */
SCRIPT_API(SetMapDebugLevel, bool(int level))
{
	if (!gIndexWeaverStore)
	{
		return false;
	}

	// Validate bounds manually to prevent invalid enum casts
	if (level < 0 || level > 2)
	{
		return false;
	}

	gIndexWeaverStore->setDebugLevel(
		static_cast<index_weaver::IndexRegistryStore::DebugLevel>(level));

	return true;
}

/**
 * @brief Retrieves the current internal verbosity level of the component.
 *
 * @return The current debug level as an integer (0, 1, or 2).
 */
SCRIPT_API(GetMapDebugLevel, int())
{
	if (!gIndexWeaverStore)
	{
		return 0;
	}

	return static_cast<int>(gIndexWeaverStore->debugLevel());
}