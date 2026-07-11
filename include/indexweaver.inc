#if defined INDEX_WEAVER_INCLUDED
    #endinput
#endif
#define INDEX_WEAVER_INCLUDED

/*
 * =============================================================================
 * IndexWeaver
 * A high-performance index registry plugin for open.mp.
 *
 * Powered by robin_hood flat hashing, providing ultra-fast average constant-
 * time lookups, cache-friendly memory access, and minimal allocation overhead.
 *
 * Designed for large-scale registries where identifier-to-index lookups must
 * remain efficient even with hundreds of thousands of entries.
 * =============================================================================
 */

/*
 * Standard return code for failed or missing lookups.
 */
#define INVALID_MAP_INDEX -1

/*
 * Creates or updates a mapping between a unique identifier and an array index.
 *
 * Parameters:
 * registry_type - Registry category identifier (must be >= 0).
 * id            - Unique database or system identifier.
 * index         - Array index associated with the identifier (must be >= 0).
 *
 * Returns:
 * true if the mapping was created or updated successfully.
 * false if parameters are invalid (e.g., negative registry_type or index).
 */
native bool:SetMapIndex(registry_type, id, index);

/*
 * Retrieves the array index associated with a unique identifier.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 * id            - Unique database or system identifier.
 *
 * Returns:
 * The associated array index if the mapping exists.
 * INVALID_MAP_INDEX (-1) otherwise.
 */
native GetMapIndex(registry_type, id);

/*
 * Removes a specific mapping from a registry.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 * id            - Unique database or system identifier.
 *
 * Returns:
 * true if the mapping existed and was removed.
 * false if the mapping did not exist or parameters were invalid.
 */
native bool:RemoveMapIndex(registry_type, id);

/*
 * Checks whether a mapping exists for the given identifier.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 * id            - Unique database or system identifier.
 *
 * Returns:
 * true if the mapping exists.
 * false otherwise.
 */
native bool:HasMapIndex(registry_type, id);

/*
 * Removes every mapping stored in the specified registry and frees its memory.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 *
 * Returns:
 * true if the registry existed and was cleared.
 * false if the registry_type was invalid or not found.
 */
native bool:ClearMapRegistry(registry_type);

/*
 * Removes every mapping from every registry managed by the plugin, completely
 * resetting the internal storage to its initial state.
 *
 * Returns:
 * No return value (void).
 */
native ClearAllMapRegistries();

/*
 * Pre-allocates memory for a registry before inserting large numbers of mappings.
 *
 * Calling this function before bulk insert operations (for example, loading
 * thousands of records from MySQL or TiDB during server startup) eliminates
 * internal rehashing and memory reallocations, ensuring zero CPU stutters.
 *
 * This function does not create mappings. It only reserves physical memory for
 * the specified registry, allowing subsequent SetMapIndex() calls to execute at
 * maximum speed.
 *
 * Parameters:
 * registry_type - Registry category identifier (must be >= 0).
 * capacity      - Expected number of mappings to be stored (must be > 0).
 *
 * Returns:
 * true if memory was preallocated successfully.
 * false if capacity is out of bounds or registry_type is invalid.
 */
native bool:ReserveMapRegistry(registry_type, capacity);