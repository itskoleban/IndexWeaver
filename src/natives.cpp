#include "natives.hpp"
#include <cstdio>
#include <exception>
#include <stdexcept>
namespace index_weaver {
static_assert(sizeof(cell)==sizeof(Value), "IndexWeaver requires 32-bit Pawn cells");
namespace {
Manager* manager = nullptr; // Non-owning binding, valid only while the component is loaded.
enum class Op { Create, Destroy, Set, Insert, Index, ID, Has, Remove, Clear, Reserve, Count, Stats, Error,
    LegacySet, LegacyGet, LegacyHas, LegacyRemove, LegacyClear, LegacyClearAll, LegacyReserve,
    LegacyCount, LegacyCountAll };
cell* output(AMX* amx, cell address) noexcept {
    // Validate the complete aligned cell, excluding the unallocated heap/stack gap.
    constexpr cell bytes = sizeof(cell);
    if (address < 0 || address % bytes || amx->stp < bytes || address > amx->stp-bytes)
        return nullptr;
    if (!(address <= amx->hea-bytes || address >= amx->stk)) return nullptr;
    cell* result = nullptr;
    if (amx_GetAddr(amx,address,&result) != AMX_ERR_NONE) return nullptr;
    return result;
}
cell narrow(std::size_t n) {
    if (n > static_cast<std::size_t>(std::numeric_limits<cell>::max())) throw std::length_error("Pawn cell overflow");
    return static_cast<cell>(n);
}
template<Op operation, int arguments>
cell AMX_NATIVE_CALL native(AMX* amx, const cell* p) noexcept {
    constexpr cell failure = operation==Op::LegacyGet ? -1 : 0;
    if (!manager || !amx || !manager->loaded(amx)) return failure;
    if (!p || p[0] != arguments*static_cast<cell>(sizeof(cell))) {
        manager->error(amx,Error::InvalidArgument); return failure;
    }
    if constexpr (operation==Op::Error) return static_cast<cell>(manager->error(amx));
    manager->error(amx,Error::None);
    try {
        if constexpr (operation==Op::Create) {
            if(p[1]<0 || (p[2]!=0 && p[2]!=1)) { manager->error(amx,Error::InvalidArgument); return 0; }
            return manager->create(amx,p[2]!=0,static_cast<std::size_t>(p[1]));
        } else if constexpr (operation==Op::Destroy) return manager->destroy(amx,p[1]);
        else if constexpr (operation==Op::LegacyClearAll) { manager->clearLegacyAll(amx); return 1; }
        else if constexpr (operation==Op::LegacyCountAll) return narrow(manager->countLegacy(amx));
        else if constexpr (operation==Op::LegacyClear) {
            bool ok=manager->clearLegacy(amx,p[1]);
            if(!ok) manager->error(amx,p[1]<0?Error::InvalidArgument:Error::Missing);
            return ok;
        } else {
            constexpr bool legacy = operation>=Op::LegacySet;
            Registry* r;
            if constexpr(legacy) {
                if constexpr(operation==Op::LegacySet) {
                    if(p[3]<0) { manager->error(amx,Error::InvalidArgument); return failure; }
                }
                if constexpr(operation==Op::LegacyReserve) {
                    // Preserve the 1.x compatibility reservation contract.
                    if(p[2]<=0 || p[2]>10000000) { manager->error(amx,Error::InvalidArgument); return failure; }
                }
                r=manager->legacy(amx,p[1],operation==Op::LegacySet || operation==Op::LegacyReserve);
            } else r=manager->get(amx,p[1]);
            if(!r) return failure;
            if constexpr(operation==Op::Set || operation==Op::Insert || operation==Op::LegacySet) {
                auto e=r->set(p[2],p[3],operation==Op::Insert); manager->error(amx,e); return e==Error::None;
            } else if constexpr(operation==Op::Index || operation==Op::ID) {
                auto* target=output(amx,p[3]);
                if(!target) { manager->error(amx,Error::InvalidReference); return 0; }
                if constexpr(operation==Op::ID) {
                    if(!r->bidirectional()) { manager->error(amx,Error::NotBidirectional); return 0; }
                    if(p[2]<0) { manager->error(amx,Error::InvalidArgument); return 0; }
                }
                auto result=operation==Op::Index?r->index(p[2]):r->id(p[2]);
                if(!result) { manager->error(amx,Error::Missing); return 0; }
                *target=*result; return 1;
            } else if constexpr(operation==Op::Has || operation==Op::LegacyHas || operation==Op::LegacyGet) {
                auto result=r->index(p[2]);
                if(!result) { manager->error(amx,Error::Missing); return failure; }
                if constexpr(operation==Op::LegacyGet) return *result;
                return 1;
            } else if constexpr(operation==Op::Remove || operation==Op::LegacyRemove) {
                bool ok=r->remove(p[2]); if(!ok) manager->error(amx,Error::Missing); return ok;
            } else if constexpr(operation==Op::Clear) {
                if(p[2]!=0 && p[2]!=1) { manager->error(amx,Error::InvalidArgument); return 0; }
                r->clear(p[2]!=0); return 1;
            } else if constexpr(operation==Op::Reserve || operation==Op::LegacyReserve) {
                if(p[2]<0) { manager->error(amx,Error::InvalidArgument); return 0; }
                r->reserve(static_cast<std::size_t>(p[2])); return 1;
            } else if constexpr(operation==Op::Count || operation==Op::LegacyCount) return narrow(r->size());
            else if constexpr(operation==Op::Stats) {
                auto* count=output(amx,p[2]); auto* buckets=output(amx,p[3]); auto* reverse=output(amx,p[4]);
                if(!count || !buckets || !reverse || count==buckets || count==reverse || buckets==reverse) {
                    manager->error(amx,Error::InvalidReference); return 0;
                }
                auto c=narrow(r->size()); auto b=narrow(r->buckets());
                *count=c; *buckets=b; *reverse=r->bidirectional(); return 1;
            }
        }
    } catch(const std::bad_alloc&) { manager->error(amx,Error::Allocation); }
    catch(const std::length_error&) { manager->error(amx,Error::Capacity); }
    catch(const std::exception& e) {
        std::fprintf(stderr,"[IndexWeaver] unexpected native exception: %s\n",e.what());
        manager->error(amx,Error::Internal);
    } catch(...) {
        std::fputs("[IndexWeaver] unexpected non-standard native exception\n",stderr);
        manager->error(amx,Error::Internal);
    }
    return failure;
}
#define N(name, op, count) {name, &native<Op::op,count>}
const AMX_NATIVE_INFO natives[] = {
    N("Registry_Create",Create,2), N("Registry_Destroy",Destroy,1), N("Registry_Set",Set,3),
    N("Registry_Insert",Insert,3), N("Registry_TryGetIndex",Index,3), N("Registry_TryGetID",ID,3),
    N("Registry_Has",Has,2), N("Registry_Remove",Remove,2), N("Registry_Clear",Clear,2),
    N("Registry_Reserve",Reserve,2), N("Registry_Count",Count,1), N("Registry_GetStats",Stats,4),
    N("Registry_GetLastError",Error,0), N("SetMapIndex",LegacySet,3), N("GetMapIndex",LegacyGet,2),
    N("HasMapIndex",LegacyHas,2), N("RemoveMapIndex",LegacyRemove,2), N("ClearMapRegistry",LegacyClear,1),
    N("ClearAllMapRegistries",LegacyClearAll,0), N("ReserveMapRegistry",LegacyReserve,2),
    N("CountMapRegistry",LegacyCount,1), N("CountAllMapRegistries",LegacyCountAll,0)
};
#undef N
}
void bindNatives(Manager* value) noexcept { manager=value; }
int registerNatives(AMX* amx) { return amx_Register(amx,natives,static_cast<int>(std::size(natives))); }
}
