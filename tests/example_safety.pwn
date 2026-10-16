#define FILTERSCRIPT
#include <open.mp>
#include <indexweaver>

// Compile and execute the actual example helpers. Intercept only their create
// and reserve calls to inject a deterministic failure at each call boundary.
new g_CreateCalls, g_FailCreate;
new bool:g_FailReserve;
stock Registry:TestRegistryCreate(capacity = 0, bool:bidirectional = false)
{
    if (++g_CreateCalls == g_FailCreate)
        return Registry_Create(-1, bidirectional);
    return Registry_Create(capacity, bidirectional);
}
stock bool:TestRegistryReserve(Registry:registry, capacity)
{
    return Registry_Reserve(registry, g_FailReserve ? -1 : capacity);
}
forward ExampleOnGameModeInit();
forward ExampleOnGameModeExit();
forward ExampleOnPlayerDisconnect(playerid, reason);
#define main stock ExampleMain
#define OnGameModeInit ExampleOnGameModeInit
#define OnGameModeExit ExampleOnGameModeExit
#define OnPlayerDisconnect ExampleOnPlayerDisconnect
#define Registry_Create TestRegistryCreate
#define Registry_Reserve TestRegistryReserve
#include "../examples/mappings.pwn"
#undef Registry_Reserve
#undef Registry_Create
#undef OnPlayerDisconnect
#undef OnGameModeExit
#undef OnGameModeInit
#undef main

#define REQUIRE(%0) if (!(%0)) return FailExample(__line)
stock FailExample(line)
{
    printf("INDEXWEAVER_EXAMPLE_FAIL line=%d error=%d", line, _:Registry_GetLastError());
    SendRconCommand("exit");
    return 0;
}
stock bool:AllExampleHandlesInvalid()
{
    return g_Accounts == INVALID_REGISTRY && g_Vehicles == INVALID_REGISTRY &&
        g_Properties == INVALID_REGISTRY && g_Clans == INVALID_REGISTRY;
}
public OnFilterScriptInit()
{
    for (new failure = 1; failure <= 4; ++failure)
    {
        g_CreateCalls = 0; g_FailCreate = failure;
        REQUIRE(!InitializeRegistries());
        REQUIRE(g_CreateCalls == failure);
        REQUIRE(Registry_GetLastError() == IW_ERROR_INVALID_ARGUMENT);
        new Registry:partial[4];
        partial[0] = g_Accounts; partial[1] = g_Vehicles;
        partial[2] = g_Properties; partial[3] = g_Clans;
        DestroyRegistries();
        REQUIRE(AllExampleHandlesInvalid());
        for (new i; i < sizeof partial; ++i)
        {
            if (partial[i] == INVALID_REGISTRY) continue;
            REQUIRE(Registry_Count(partial[i]) == 0);
            REQUIRE(Registry_GetLastError() == IW_ERROR_INVALID_HANDLE);
        }
        // The callback itself must also clean up a partial initialization.
        g_CreateCalls = 0;
        REQUIRE(ExampleOnGameModeInit() == 0);
        REQUIRE(g_CreateCalls == failure && AllExampleHandlesInvalid());
    }
    g_CreateCalls = 0; g_FailCreate = 0;
    REQUIRE(InitializeRegistries());
    REQUIRE(Registry_Insert(g_Properties, 42, 2));
    g_FailReserve = true;
    REQUIRE(!BeginPropertyReload(1000));
    REQUIRE(Registry_GetLastError() == IW_ERROR_INVALID_ARGUMENT);
    new index;
    REQUIRE(Registry_TryGetIndex(g_Properties, 42, index) && index == 2);
    REQUIRE(Registry_Count(g_Properties) == 1);
    g_FailReserve = false;
    REQUIRE(BeginPropertyReload(1000));
    REQUIRE(Registry_Count(g_Properties) == 0);
    DestroyRegistries();
    REQUIRE(AllExampleHandlesInvalid());
    print("INDEXWEAVER_EXAMPLE_SAFETY_PASS");
    return 1;
}
public OnFilterScriptExit()
{
    DestroyRegistries();
    return 1;
}
