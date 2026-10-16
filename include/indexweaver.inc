#if defined INDEX_WEAVER_INCLUDED
    #endinput
#endif
#define INDEX_WEAVER_INCLUDED
// Generated version line is checked against VERSION by tools/check_version.py.
#define INDEX_WEAVER_VERSION "2.0.0"
#define INVALID_REGISTRY Registry:0
#define INVALID_MAP_INDEX (-1)
#define INDEX_WEAVER_MAX_RESERVED_CAPACITY 10000000

enum IW_Error {
    IW_ERROR_NONE, IW_ERROR_INVALID_ARGUMENT, IW_ERROR_INVALID_HANDLE,
    IW_ERROR_WRONG_OWNER, IW_ERROR_MISSING, IW_ERROR_DUPLICATE_ID,
    IW_ERROR_OCCUPIED_INDEX, IW_ERROR_NOT_BIDIRECTIONAL, IW_ERROR_ALLOCATION,
    IW_ERROR_CAPACITY, IW_ERROR_HANDLE_EXHAUSTED, IW_ERROR_INVALID_REFERENCE,
    IW_ERROR_INTERNAL
};
// Each handle belongs to the creating script. Zero is invalid.
native Registry:Registry_Create(capacity = 0, bool:bidirectional = false);
native bool:Registry_Destroy(Registry:registry);
// Signed IDs, nonnegative indices. Set updates an ID; Insert rejects existing IDs.
native bool:Registry_Set(Registry:registry, id, index);
native bool:Registry_Insert(Registry:registry, id, index);
// Output remains unchanged on failure. Reverse lookup requires bidirectional mode.
native bool:Registry_TryGetIndex(Registry:registry, id, &index);
native bool:Registry_TryGetID(Registry:registry, index, &id);
native bool:Registry_Has(Registry:registry, id);
native bool:Registry_Remove(Registry:registry, id);
// Clear preserves the handle; release=true also releases hash bucket storage.
native bool:Registry_Clear(Registry:registry, bool:release = false);
native bool:Registry_Reserve(Registry:registry, capacity);
native Registry_Count(Registry:registry);
// Three distinct output variables required. Buckets describes the forward table.
native bool:Registry_GetStats(Registry:registry, &count, &buckets, &bool:bidirectional);
// Per-script error, replaced by every other native call; reading it preserves it.
native IW_Error:Registry_GetLastError();

// Deprecated 1.x mapping API. Categories are now PRIVATE TO THE CALLING SCRIPT.
// Forward-only, IDs may share an index; no cross-script access. See migration guide.
native bool:SetMapIndex(registry_type, id, index);
native GetMapIndex(registry_type, id);
native bool:RemoveMapIndex(registry_type, id);
native bool:HasMapIndex(registry_type, id);
// Destroy an existing category and release its storage, even when it is empty.
native bool:ClearMapRegistry(registry_type);
native ClearAllMapRegistries();
native bool:ReserveMapRegistry(registry_type, capacity);
native CountMapRegistry(registry_type);
native CountAllMapRegistries();
