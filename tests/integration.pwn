#include <open.mp>
#include <indexweaver>
#define REQUIRE(%0) if (!(%0)) return FailTest(__line)
main() {}
stock FailTest(line)
{
    printf("INDEXWEAVER_INTEGRATION_FAIL line=%d error=%d", line, _:Registry_GetLastError());
    SendRconCommand("exit");
    return 0;
}
public OnGameModeInit()
{
    new Registry:r = Registry_Create(100, true);
    REQUIRE(r != INVALID_REGISTRY);
    REQUIRE(Registry_Set(r, -1, 2));
    REQUIRE(!Registry_Set(r, 55, 2));
    REQUIRE(Registry_GetLastError() == IW_ERROR_OCCUPIED_INDEX);
    new index = -1, id;
    REQUIRE(Registry_TryGetIndex(r, -1, index) && index == 2);
    REQUIRE(Registry_TryGetID(r, 2, id) && id == -1);
    REQUIRE(Registry_Set(r, -1, 3));
    REQUIRE(!Registry_TryGetID(r, 2, id));
    new count, buckets, bool:reverse;
    REQUIRE(Registry_GetStats(r, count, buckets, reverse));
    REQUIRE(count == 1 && reverse && buckets > 0);
    REQUIRE(Registry_Remove(r, -1));
    REQUIRE(Registry_Clear(r, true));
    REQUIRE(Registry_Destroy(r));
    REQUIRE(!Registry_Set(r, 1, 1));
    REQUIRE(Registry_GetLastError() == IW_ERROR_INVALID_HANDLE);
    new Registry:replacement = Registry_Create();
    REQUIRE(replacement != r);
    REQUIRE(Registry_Insert(replacement, 1, 0));
    REQUIRE(!Registry_Insert(replacement, 1, 0));
    REQUIRE(Registry_GetLastError() == IW_ERROR_DUPLICATE_ID);
    REQUIRE(Registry_Destroy(replacement));
    REQUIRE(SetMapIndex(77, -20, 3));
    REQUIRE(GetMapIndex(77, -20) == 3);
    REQUIRE(ClearMapRegistry(77));
    print("INDEXWEAVER_INTEGRATION_PASS");
    // Leave an owned registry alive deliberately to exercise AMX unload cleanup.
    REQUIRE(Registry_Create() != INVALID_REGISTRY);
    #if defined IW_INTEGRATION_AUTORUN
        SetTimerEx("IntegrationStep", 100, false, "i", 0);
    #endif
    return 1;
}

// Opt-in runtime lifecycle harness. Uses scriptfiles/iw-integration-state.txt only
// in an isolated test server. Compile with IW_INTEGRATION_AUTORUN=1.
#if defined IW_INTEGRATION_AUTORUN
new Registry:g_Survivor;
new g_Foreign;
forward IntegrationStep(step);
forward IntegrationForeign(handle);
public IntegrationForeign(handle)
{
    g_Foreign = handle;
    if (Registry_Destroy(Registry:handle)) return 0;
    return Registry_GetLastError() == IW_ERROR_WRONG_OWNER;
}
public IntegrationStep(step)
{
    switch (step)
    {
        case 0:
        {
            g_Survivor = Registry_Create();
            REQUIRE(Registry_Set(g_Survivor, 789, 10));
            SendRconCommand("loadfs ownership");
            REQUIRE(g_Foreign > 0);
            SetTimerEx("IntegrationStep", 100, false, "i", 1);
        }
        case 1:
        {
            SendRconCommand("unloadfs ownership");
            REQUIRE(Registry_Count(Registry:g_Foreign) == 0);
            REQUIRE(Registry_GetLastError() == IW_ERROR_INVALID_HANDLE);
            new index;
            REQUIRE(Registry_TryGetIndex(g_Survivor, 789, index) && index == 10);
            print("INDEXWEAVER_FILTERSCRIPT_UNLOAD_PASS");
            SetTimerEx("IntegrationStep", 100, false, "i", 2);
        }
        case 2:
        {
            new cycle, previous;
            if (fexist("iw-integration-state.txt"))
            {
                new File:input = fopen("iw-integration-state.txt", io_read);
                REQUIRE(input != File:0);
                new line[40];
                fread(input, line); cycle = strval(line);
                fread(input, line); previous = strval(line);
                fclose(input);
                REQUIRE(Registry_Count(Registry:previous) == 0);
                REQUIRE(Registry_GetLastError() == IW_ERROR_INVALID_HANDLE);
                REQUIRE(_:g_Survivor > previous);
                print("INDEXWEAVER_GMX_HANDLE_PASS");
            }
            new File:output_file = fopen("iw-integration-state.txt", io_write);
            REQUIRE(output_file != File:0);
            new text[80];
            format(text, sizeof text, "%d\n%d\n", cycle + 1, _:g_Survivor);
            fwrite(output_file, text); fclose(output_file);
            if (cycle < 2) SendRconCommand("gmx");
            else
            {
                print("INDEXWEAVER_LIFECYCLE_PASS");
                SendRconCommand("exit");
            }
        }
    }
    return 1;
}
#endif
