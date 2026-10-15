#include <open.mp>
#include <indexweaver>
#define MAX_PROPERTIES (1000)
#define MAX_CLANS (200)
new Registry:g_Accounts = INVALID_REGISTRY;
new Registry:g_Vehicles = INVALID_REGISTRY;
new Registry:g_Properties = INVALID_REGISTRY;
new Registry:g_Clans = INVALID_REGISTRY;
new g_AccountID[MAX_PLAYERS];
new bool:g_HasAccount[MAX_PLAYERS];
main() {}
public OnGameModeInit()
{
    g_Accounts = Registry_Create(MAX_PLAYERS, true);
    g_Vehicles = Registry_Create(MAX_VEHICLES, true);
    g_Properties = Registry_Create(MAX_PROPERTIES);
    g_Clans = Registry_Create(MAX_CLANS);
    if (g_Accounts == INVALID_REGISTRY || g_Vehicles == INVALID_REGISTRY ||
        g_Properties == INVALID_REGISTRY || g_Clans == INVALID_REGISTRY)
    {
        printf("Registry initialization failed: %d", _:Registry_GetLastError());
        return 0;
    }
    return 1;
}
// Call after the database plugin has validated login and the current session.
// The database request must carry your own connection/login generation token.
stock bool:RegisterAccount(playerid, database_id)
{
    if (playerid < 0 || playerid >= MAX_PLAYERS || !IsPlayerConnected(playerid)) return false;
    if (g_HasAccount[playerid]) return false;
    if (!Registry_Insert(g_Accounts, database_id, playerid)) return false;
    g_AccountID[playerid] = database_id;
    g_HasAccount[playerid] = true;
    return true;
}
public OnPlayerDisconnect(playerid, reason)
{
    #pragma unused reason
    if (g_HasAccount[playerid]) Registry_Remove(g_Accounts, g_AccountID[playerid]);
    g_HasAccount[playerid] = false;
    return 1;
}
stock bool:FindAccount(database_id, &playerid)
{
    return Registry_TryGetIndex(g_Accounts, database_id, playerid);
}
stock CreateDatabaseVehicle(database_id, model, Float:x, Float:y, Float:z)
{
    new vehicleid = CreateVehicle(model, x, y, z, 0.0, -1, -1, -1);
    if (vehicleid == INVALID_VEHICLE_ID || vehicleid == 0) return INVALID_VEHICLE_ID;
    if (!Registry_Insert(g_Vehicles, database_id, vehicleid))
    {
        DestroyVehicle(vehicleid);
        return INVALID_VEHICLE_ID;
    }
    return vehicleid;
}
// Call this wrapper whenever your script destroys a mapped vehicle.
// Vehicle death and vehicle destruction are distinct; no OnVehicleDestroy callback exists.
stock bool:DestroyDatabaseVehicle(vehicleid)
{
    new database_id;
    if (!Registry_TryGetID(g_Vehicles, vehicleid, database_id)) return false;
    if (!DestroyVehicle(vehicleid)) return false;
    return Registry_Remove(g_Vehicles, database_id);
}
// Adapters for each row of an asynchronous query result, after session checks.
// IndexWeaver does not depend on any MySQL/PostgreSQL API.
stock bool:RegisterPropertyRow(database_id, slot)
{
    if (slot < 0 || slot >= MAX_PROPERTIES) return false;
    return Registry_Insert(g_Properties, database_id, slot);
}
stock bool:RegisterClanRow(database_id, slot)
{
    if (slot < 0 || slot >= MAX_CLANS) return false;
    return Registry_Insert(g_Clans, database_id, slot);
}
stock bool:BeginPropertyReload(expected_rows)
{
    if (expected_rows < 0 || expected_rows > MAX_PROPERTIES) return false;
    if (!Registry_Clear(g_Properties)) return false;
    return Registry_Reserve(g_Properties, expected_rows);
}
public OnGameModeExit()
{
    // Explicit cleanup permits deterministic early release. AMX unload also cleans up.
    if (g_Accounts != INVALID_REGISTRY) Registry_Destroy(g_Accounts);
    if (g_Vehicles != INVALID_REGISTRY) Registry_Destroy(g_Vehicles);
    if (g_Properties != INVALID_REGISTRY) Registry_Destroy(g_Properties);
    if (g_Clans != INVALID_REGISTRY) Registry_Destroy(g_Clans);
    g_Accounts = g_Vehicles = g_Properties = g_Clans = INVALID_REGISTRY;
    return 1;
}
