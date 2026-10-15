#include "natives.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace index_weaver;
#define CHECK(...) do { if(!(__VA_ARGS__)) { std::cerr << "native check line " << __LINE__ << ": " << #__VA_ARGS__ << '\n'; std::exit(1); } } while(false)
static std::map<std::string, AMX_NATIVE> functions;
int AMXAPI amx_Register(AMX*, const AMX_NATIVE_INFO* entries, int count) {
    for(int i=0;i<count;++i) CHECK(functions.emplace(entries[i].name,entries[i].func).second);
    return AMX_ERR_NONE;
}
int AMXAPI amx_GetAddr(AMX* amx, cell addr, cell** out) { *out=reinterpret_cast<cell*>(amx->data+addr); return AMX_ERR_NONE; }
cell call(AMX& amx, const char* name, std::initializer_list<cell> args) {
    std::vector<cell> p; p.push_back(static_cast<cell>(args.size()*sizeof(cell))); p.insert(p.end(),args);
    return functions.at(name)(&amx,p.data());
}
int main() {
    Manager m; AMX a{}, b{}; cell data[32]{};
    a.data=reinterpret_cast<unsigned char*>(data); a.hea=64; a.stk=96; a.stp=128;
    b=a; m.load(&a); m.load(&b); bindNatives(&m); CHECK(registerNatives(&a)==0); CHECK(functions.size()==22);
    auto h=call(a,"Registry_Create",{0,1}); CHECK(h>0);
    CHECK(call(a,"Registry_Set",{h,-10,2})==1);
    CHECK(call(a,"Registry_Insert",{h,-10,3})==0);
    CHECK(call(a,"Registry_GetLastError",{})==static_cast<cell>(Error::DuplicateID));
    CHECK(call(a,"Registry_Set",{h,99,2})==0);
    CHECK(call(a,"Registry_GetLastError",{})==static_cast<cell>(Error::OccupiedIndex));
    CHECK(call(a,"Registry_TryGetIndex",{h,-10,0})==1); CHECK(data[0]==2);
    CHECK(call(a,"Registry_TryGetID",{h,2,4})==1); CHECK(data[1]==-10);
    for(cell addr : {-4,1,61,64,68,92,125,128,2147483647}) {
        CHECK(call(a,"Registry_TryGetIndex",{h,-10,addr})==0);
        CHECK(call(a,"Registry_GetLastError",{})==static_cast<cell>(Error::InvalidReference));
    }
    CHECK(call(a,"Registry_TryGetIndex",{h,-10,96})==1); CHECK(data[24]==2);
    data[0]=123; CHECK(call(a,"Registry_TryGetIndex",{h,999,0})==0); CHECK(data[0]==123);
    CHECK(call(a,"Registry_GetStats",{h,0,4,8})==1); CHECK(data[0]==1 && data[2]==1);
    CHECK(call(a,"Registry_GetStats",{h,0,0,8})==0);
    CHECK(call(a,"Registry_Has",{h,-10})==1); CHECK(call(a,"Registry_Count",{h})==1);
    CHECK(call(b,"Registry_Destroy",{h})==0); CHECK(call(b,"Registry_GetLastError",{})==static_cast<cell>(Error::WrongOwner));
    CHECK(call(a,"Registry_Set",{h,1})==0); CHECK(call(a,"Registry_Set",{h,1,2,3})==0);
    CHECK(call(a,"Registry_Reserve",{h,-1})==0); CHECK(call(a,"Registry_Reserve",{h,100})==1);
    CHECK(call(a,"Registry_Remove",{h,-10})==1); CHECK(call(a,"Registry_Remove",{h,-10})==0);
    CHECK(call(a,"Registry_Clear",{h,2})==0); CHECK(call(a,"Registry_Clear",{h,1})==1);
    CHECK(call(a,"Registry_Destroy",{h})==1); CHECK(call(a,"Registry_Destroy",{h})==0);
    auto f=call(a,"Registry_Create",{0,0}); CHECK(call(a,"Registry_TryGetID",{f,0,0})==0);
    CHECK(call(a,"Registry_GetLastError",{})==static_cast<cell>(Error::NotBidirectional));
    CHECK(call(a,"Registry_Create",{-1,0})==0); CHECK(call(a,"Registry_Create",{0,2})==0);
    CHECK(call(a,"SetMapIndex",{77,-3,0})==1); CHECK(call(a,"SetMapIndex",{77,5,0})==1);
    CHECK(call(a,"GetMapIndex",{77,-3})==0); CHECK(call(b,"GetMapIndex",{77,-3})==-1);
    CHECK(call(a,"HasMapIndex",{77,-3})==1); CHECK(call(a,"CountMapRegistry",{77})==2);
    CHECK(call(a,"CountAllMapRegistries",{})==1); CHECK(call(a,"RemoveMapIndex",{77,-3})==1);
    CHECK(call(a,"ReserveMapRegistry",{77,0})==0); CHECK(call(a,"ReserveMapRegistry",{77,10000001})==0);
    CHECK(call(a,"ReserveMapRegistry",{77,100})==1); CHECK(call(a,"ClearMapRegistry",{77})==1);
    CHECK(call(a,"ClearMapRegistry",{77})==0); CHECK(call(a,"ClearAllMapRegistries",{})==1);
    m.reset(); CHECK(call(a,"Registry_Count",{f})==0); CHECK(call(a,"Registry_GetLastError",{})==static_cast<cell>(Error::InvalidHandle));
    m.unload(&a); CHECK(call(a,"Registry_Create",{0,0})==0); m.load(&a);
    CHECK(call(a,"Registry_Create",{0,0})>f); bindNatives(nullptr);
    CHECK(call(a,"GetMapIndex",{77,0})==-1);
    std::cout << "PASS: all 22 natives, AMX address validation, argument counts, ownership and reset (mock AMX)\n";
}
