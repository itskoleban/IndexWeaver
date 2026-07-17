#if defined INDEX_WEAVER_INCLUDED
    #endinput
#endif
#define INDEX_WEAVER_INCLUDED

/*
 * =============================================================================
 * IndexWeaver
 *
 * A lightweight in-memory index registry component for open.mp.
 *
 * Stores direct mappings from an identifier to the array index where an object
 * already exists in memory. The plugin never stores the object itself, only
 * the identifier-to-index association for fast lookups.
 *
 * Features:
 *   - Average O(1) insertion, lookup and removal.
 *   - Multiple independent registries.
 *   - Optional capacity reservation for bulk loading.
 *   - Runtime configurable debug logging.
 * =============================================================================
 */

#define INDEX_WEAVER_VERSION "1.1.0"

/*
 * Maximum capacity accepted by ReserveMapRegistry().
 */
#define INDEX_WEAVER_MAX_RESERVED_CAPACITY 10000000

/*
 * Standard return value for failed or missing lookups.
 */
#define INVALID_MAP_INDEX -1

/*
 * Debug logging levels.
 */
#define INDEX_WEAVER_DEBUG_OFF 0
#define INDEX_WEAVER_DEBUG_ERRORS 1
#define INDEX_WEAVER_DEBUG_VERBOSE 2

/*
 * Sets the plugin debug logging level.
 *
 * Parameters:
 * level - One of the following:
 *         INDEX_WEAVER_DEBUG_OFF
 *             Disables all debug logging.
 *
 *         INDEX_WEAVER_DEBUG_ERRORS
 *             Logs invalid parameters, allocation failures and unexpected
 *             internal errors.
 *
 *         INDEX_WEAVER_DEBUG_VERBOSE
 *             Logs all registry operations in addition to errors.
 *
 * Returns:
 * true if the debug level was updated successfully.
 * false if the plugin is unavailable or the supplied level is invalid.
 *
 * Remarks:
 * Debug logging is disabled by default.
 * Verbose mode is intended for development and troubleshooting and may
 * generate a large amount of console output.
 */
native bool:SetMapDebugLevel(level);

/*
 * Returns the currently active debug logging level.
 *
 * Returns:
 * INDEX_WEAVER_DEBUG_OFF
 * INDEX_WEAVER_DEBUG_ERRORS
 * INDEX_WEAVER_DEBUG_VERBOSE
 *
 * Remarks:
 * Returns INDEX_WEAVER_DEBUG_OFF if the plugin is unavailable.
 */
native GetMapDebugLevel();

/*
 * Creates a new mapping or updates an existing one.
 *
 * Parameters:
 * registry_type - Registry category identifier (must be >= 0).
 * id            - Signed identifier associated with the object.
 * index         - Array index where the object is stored (must be >= 0).
 *
 * Returns:
 * true if the mapping was created or updated successfully.
 * false if the supplied parameters are invalid or the operation failed.
 *
 * Complexity:
 * Average O(1).
 */
native bool:SetMapIndex(registry_type, id, index);

/*
 * Retrieves the array index associated with an identifier.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 * id            - Signed identifier to search for.
 *
 * Returns:
 * The associated array index if the mapping exists.
 * INVALID_MAP_INDEX otherwise.
 *
 * Complexity:
 * Average O(1).
 */
native GetMapIndex(registry_type, id);

/*
 * Removes a mapping from the specified registry.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 * id            - Signed identifier to remove.
 *
 * Returns:
 * true if the mapping existed and was removed.
 * false otherwise.
 *
 * Complexity:
 * Average O(1).
 */
native bool:RemoveMapIndex(registry_type, id);

/*
 * Checks whether a mapping exists.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 * id            - Signed identifier to search for.
 *
 * Returns:
 * true if the mapping exists.
 * false otherwise.
 *
 * Complexity:
 * Average O(1).
 */
native bool:HasMapIndex(registry_type, id);

/*
 * Removes every mapping stored in the specified registry.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 *
 * Returns:
 * true if the registry existed and was cleared successfully.
 * false if the registry does not exist or the supplied registry type is
 * invalid.
 *
 * Complexity:
 * O(n), where n is the number of mappings stored in the specified registry.
 */
native bool:ClearMapRegistry(registry_type);

/*
 * Removes every mapping from every registry managed by the plugin.
 *
 * Returns:
 * Nothing.
 *
 * Remarks:
 * Restores the plugin to an empty state.
 *
 * Complexity:
 * O(N), where N is the total number of stored mappings.
 */
native ClearAllMapRegistries();

/*
 * Retrieves the total number of mappings stored in a specific registry.
 *
 * Parameters:
 * registry_type - Registry category identifier.
 *
 * Returns:
 * The number of elements currently mapped in the registry.
 *
 * Complexity:
 * O(1).
 */
native CountMapRegistry(registry_type);

/*
 * Retrieves the total number of active registries.
 *
 * An active registry is one that contains at least one mapping.
 *
 * Returns:
 * The total number of active registries in the storage engine.
 *
 * Complexity:
 * O(M), where M is the total number of existing registries.
 */
native CountAllMapRegistries();

/*
 * Preallocates storage for a registry before bulk insertion.
 *
 * Reserving capacity in advance reduces reallocations and rehashing while
 * inserting large numbers of mappings, improving loading performance.
 *
 * Parameters:
 * registry_type - Registry category identifier (must be >= 0).
 * capacity      - Expected number of mappings.
 *
 * Returns:
 * true if the reservation succeeded.
 * false if:
 *   - registry_type is invalid.
 *   - capacity is 0.
 *   - capacity exceeds INDEX_WEAVER_MAX_RESERVED_CAPACITY.
 *   - the reservation could not be completed.
 *
 * Complexity:
 * Average O(1).
 */
native bool:ReserveMapRegistry(registry_type, capacity);

/*
 * Contract Summary
 * ----------------
 *
 * - registry_type must be >= 0.
 * - index must be >= 0.
 * - id may be any signed integer.
 * - GetMapIndex() returns INVALID_MAP_INDEX when a mapping does not exist.
 * - ReserveMapRegistry() rejects capacities equal to 0 or greater than
 *   INDEX_WEAVER_MAX_RESERVED_CAPACITY.
 * - Debug logging is disabled by default and can be changed at runtime
 *   through SetMapDebugLevel().
 */