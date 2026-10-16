#define FILTERSCRIPT
#include <open.mp>
#include <indexweaver>
#define REQUIRE(%0) if (!(%0)) return FailSurvivor(__line)
new Registry:g_Owned = INVALID_REGISTRY;
new g_Checks;
stock FailSurvivor(line)
{
    printf("INDEXWEAVER_SURVIVOR_FAIL line=%d checks=%d error=%d", line, g_Checks, _:Registry_GetLastError());
    SendRconCommand("exit");
    return 0;
}
public OnFilterScriptInit()
{
    // Create mappings only after initial gamemode initialization has completed.
    return 1;
}
forward SurvivorCheck(cycle);
public SurvivorCheck(cycle)
{
    // A reloaded filterscript would lose this counter and fail on cycle 1/2.
    REQUIRE(cycle == g_Checks);
    new Registry:previous = g_Owned;
    if (g_Checks > 0)
    {
        REQUIRE(Registry_Count(previous) == 0);
        REQUIRE(Registry_GetLastError() == IW_ERROR_INVALID_HANDLE);
        REQUIRE(GetMapIndex(81, -25) == INVALID_MAP_INDEX);
        REQUIRE(Registry_GetLastError() == IW_ERROR_MISSING);
        print("INDEXWEAVER_SURVIVOR_RESET_PASS");
    }
    g_Owned = Registry_Create(10, true);
    REQUIRE(g_Owned != INVALID_REGISTRY && _:g_Owned > _:previous);
    REQUIRE(Registry_Insert(g_Owned, -25, 7));
    REQUIRE(SetMapIndex(81, -25, 7));
    ++g_Checks;
    print("INDEXWEAVER_SURVIVOR_READY_PASS");
    return 1;
}
forward SurvivorVerify(cycle);
public SurvivorVerify(cycle)
{
    REQUIRE(g_Checks == cycle + 1);
    new index, id;
    REQUIRE(Registry_TryGetIndex(g_Owned, -25, index) && index == 7);
    REQUIRE(Registry_TryGetID(g_Owned, 7, id) && id == -25);
    REQUIRE(GetMapIndex(81, -25) == 7);
    return 1;
}
public OnFilterScriptExit()
{
    REQUIRE(g_Checks == 3);
    print("INDEXWEAVER_SURVIVOR_SHUTDOWN_PASS");
    return 1;
}
