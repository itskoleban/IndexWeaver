#if defined INDEX_WEAVER_INCLUDED
    #endinput
#endif
#define INDEX_WEAVER_INCLUDED

/*
 * =============================================================================
 *  IndexWeaver
 *  A high-performance index registry plugin for open.mp.
 *
 *  Powered by robin_hood flat hashing, providing ultra-fast average constant-
 *  time lookups, cache-friendly memory access, and minimal allocation overhead.
 *
 *  Designed for large-scale registries where identifier-to-index lookups must
 *  remain efficient even with hundreds of thousands of entries.
 * =============================================================================
 */

/*
 * Common return codes.
 */
#define INVALID_MAP_INDEX -1
#define INDEX_WEAVER_SUCCESS 1

/*
 * Creates or updates a mapping between a unique identifier and an index.
 *
 * Parameters:
 *   registry_type - Registry identifier.
 *   id            - Unique identifier.
 *   index         - Index associated with the identifier.
 *
 * Returns:
 *   INDEX_WEAVER_SUCCESS.
 */
native SetMapIndex(registry_type, id, index);

/*
 * Retrieves the index associated with a unique identifier.
 *
 * Parameters:
 *   registry_type - Registry identifier.
 *   id            - Unique identifier.
 *
 * Returns:
 *   The associated index if the mapping exists.
 *   INVALID_MAP_INDEX otherwise.
 */
native GetMapIndex(registry_type, id);

/*
 * Removes a mapping from a registry.
 *
 * Parameters:
 *   registry_type - Registry identifier.
 *   id            - Unique identifier.
 *
 * Returns:
 *   true if the mapping existed and was removed.
 *   false otherwise.
 */
native bool:RemoveMapIndex(registry_type, id);

/*
 * Checks whether a mapping exists.
 *
 * Parameters:
 *   registry_type - Registry identifier.
 *   id            - Unique identifier.
 *
 * Returns:
 *   true if the mapping exists.
 *   false otherwise.
 */
native bool:HasMapIndex(registry_type, id);

/*
 * Removes every mapping stored in the specified registry.
 *
 * Parameters:
 *   registry_type - Registry identifier.
 *
 * Returns:
 *   INDEX_WEAVER_SUCCESS.
 */
native ClearMapRegistry(registry_type);

/*
 * Removes every mapping from every registry managed by the plugin.
 *
 * Returns:
 *   INDEX_WEAVER_SUCCESS.
 */
native ClearAllMapRegistries();

/*
 * Reserves memory for a registry before inserting large numbers of mappings.
 *
 * Calling this function before bulk insert operations (for example, loading
 * thousands of records from MySQL or TiDB during server startup) minimizes
 * internal rehashing and memory reallocations, significantly reducing CPU
 * overhead during insertion.
 *
 * This function does not create mappings. It only preallocates storage for the
 * specified registry, allowing subsequent SetMapIndex() calls to execute with
 * fewer allocation events.
 *
 * Parameters:
 *   registry_type - Registry identifier.
 *   capacity      - Expected number of mappings to be stored.
 *
 * Returns:
 *   INDEX_WEAVER_SUCCESS.
 */
native ReserveMapRegistry(registry_type, capacity);