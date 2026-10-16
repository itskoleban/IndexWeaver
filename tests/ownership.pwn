#define FILTERSCRIPT
#include <open.mp>
#include <indexweaver>
new Registry:g_Owned;
public OnFilterScriptInit()
{
    g_Owned = Registry_Create(5, true);
    if (g_Owned == INVALID_REGISTRY || !Registry_Set(g_Owned, 123, 456) ||
        CallRemoteFunction("IntegrationForeign", "i", _:g_Owned) != 1)
    {
        print("INDEXWEAVER_FILTERSCRIPT_FAIL");
        return 0;
    }
    print("INDEXWEAVER_FILTERSCRIPT_OWNERSHIP_PASS");
    return 1;
}
public OnFilterScriptExit()
{
    // Leave the handle alive to test owner cleanup after this callback returns.
    return 1;
}
