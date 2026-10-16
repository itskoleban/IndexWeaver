#include "manager.hpp"
#include "diagnostics.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <new>
#ifdef IW_TEST_NATIVE_ALLOCATION
#include "natives.hpp"
#include <cstring>
#endif
#ifdef _WIN32
#include <malloc.h>
#endif

// Process-local, single-threaded fault injection. Include aligned allocation:
// the default PMR resource may use it even for ordinarily aligned map nodes.
namespace {
int remaining = -1;
std::size_t live = 0;
void beforeAllocation() {
    if (remaining == 0) throw std::bad_alloc();
    if (remaining > 0) --remaining;
}
void* allocate(std::size_t size) {
    beforeAllocation();
    if (auto* p = std::malloc(size ? size : 1)) { ++live; return p; }
    throw std::bad_alloc();
}
void* allocateAligned(std::size_t size, std::size_t alignment) {
    beforeAllocation();
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
#ifdef _WIN32
    auto* p = _aligned_malloc(size ? size : 1, alignment);
#else
    void* p = nullptr;
    if (posix_memalign(&p, alignment, size ? size : 1) != 0) p = nullptr;
#endif
    if (!p) throw std::bad_alloc();
    ++live; return p;
}
void release(void* p) noexcept {
    if (p) { --live; std::free(p); }
}
void releaseAligned(void* p) noexcept {
    if (!p) return;
    --live;
#ifdef _WIN32
    _aligned_free(p);
#else
    std::free(p);
#endif
}
}
void* operator new(std::size_t n) { return allocate(n); }
void* operator new[](std::size_t n) { return allocate(n); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void* operator new(std::size_t n, std::align_val_t a) { return allocateAligned(n, static_cast<std::size_t>(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return allocateAligned(n, static_cast<std::size_t>(a)); }
void operator delete(void* p, std::align_val_t) noexcept { releaseAligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { releaseAligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { releaseAligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { releaseAligned(p); }

using namespace index_weaver;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::fprintf(stderr, "allocation check line %d: %s\n", __LINE__, #__VA_ARGS__); std::exit(1); } } while (false)

// Rebuild the exact initial state for every allocation boundary. Stop only
// after one complete execution without a failure (the upper bound guards bugs).
template<class Test> void sweep(Test test) {
    for (int budget = 0; budget < 128; ++budget) {
        const auto baseline = live;
        const bool completed = test(budget);
        CHECK(remaining == -1);
        CHECK(live == baseline);
        if (completed) {
            std::printf("  verified %d failing allocation boundaries before successful execution\n", budget);
            return;
        }
    }
    CHECK(false);
}

void creation(bool reverse, std::size_t capacity) {
    sweep([=](int budget) {
        Manager m; int owner = 0, other = 0;
        m.load(&owner); m.load(&other);
        auto survivor = m.create(&other, true, 1);
        CHECK(m.get(&other, survivor)->set(9, 7) == Error::None);
        auto previous = m.create(&owner, false, 0);
        CHECK(m.destroy(&owner, previous));
        Handle created = 0; bool failed = false;
        remaining = budget;
        try { created = m.create(&owner, reverse, capacity); }
        catch (const std::bad_alloc&) { failed = true; }
        remaining = -1;
        CHECK(m.loaded(&owner)); CHECK(!m.get(&owner, previous));
        CHECK(m.get(&other, survivor)->index(9) == 7);
        CHECK(m.get(&other, survivor)->invariant());
        if (failed) {
            CHECK(created == 0);
            // Failed creation must neither publish nor consume a handle.
            CHECK(!m.get(&owner, previous + 1));
            created = m.create(&owner, reverse, capacity);
        }
        CHECK(created == previous + 1);
        CHECK(m.get(&owner, created)->size() == 0);
        CHECK(m.get(&owner, created)->bidirectional() == reverse);
        CHECK(m.get(&owner, created)->set(5, 6) == Error::None);
        CHECK(!m.destroy(&other, created));
        CHECK(m.destroy(&owner, created));
        return !failed;
    });
}
void loading(bool repeat) {
    sweep([=](int budget) {
        Manager m; int owner = 0, other = 0;
        m.load(&other); auto survivor = m.create(&other, false, 0);
        Handle old = 0;
        if (repeat) {
            m.load(&owner); old = m.create(&owner, true, 2);
            CHECK(m.legacy(&owner, 1, true)->set(1, 2) == Error::None);
        }
        bool failed = false;
        remaining = budget;
        try { m.load(&owner); } catch (const std::bad_alloc&) { failed = true; }
        remaining = -1;
        CHECK(m.loaded(&owner) == !failed);
        CHECK(!m.get(&owner, old)); CHECK(m.countLegacy(&owner) == 0);
        CHECK(m.get(&other, survivor));
        if (failed) m.load(&owner);
        CHECK(m.loaded(&owner)); CHECK(m.create(&owner, false, 0) > old);
        return !failed;
    });
}
void compatibility() {
    sweep([](int budget) {
        Manager m; int owner = 0; m.load(&owner);
        auto h = m.create(&owner, true, 1);
        CHECK(m.get(&owner, h)->set(3, 4) == Error::None);
        bool failed = false;
        remaining = budget;
        try { CHECK(m.legacy(&owner, 100, true)->set(7, 8) == Error::None); }
        catch (const std::bad_alloc&) { failed = true; }
        remaining = -1;
        CHECK(m.get(&owner, h)->index(3) == 4);
        auto* r = m.legacy(&owner, 100, false);
        if (failed) {
            CHECK(!r || (r->size() == 0 && r->invariant()));
            CHECK(m.countLegacy(&owner) == 0);
            CHECK(m.legacy(&owner, 100, true)->set(7, 8) == Error::None);
        }
        CHECK(m.legacy(&owner, 100, false)->index(7) == 8);
        CHECK(m.countLegacy(&owner) == 1);
        // Unload/reset/shutdown must require no further allocations.
        remaining = 0;
        m.unload(&owner); m.reset(); m.shutdown();
        remaining = -1;
        return !failed;
    });
}
void emptyLegacyCleanup() {
    const auto baseline = live;
    {
        Manager m; int owner = 0, other = 0;
        m.load(&owner); m.load(&other);
        CHECK(m.legacy(&other, 77, true)->set(9, 2) == Error::None);
        auto* reserved = m.legacy(&owner, 77, true);
        reserved->reserve(10000);
        CHECK(m.countLegacy(&owner) == 0);
        const auto allocated = live;
        remaining = 0;
        CHECK(m.clearLegacy(&owner, 77));
        CHECK(!m.clearLegacy(&owner, 77));
        remaining = -1;
        CHECK(live < allocated);
        CHECK(!m.legacy(&owner, 77, false));
        CHECK(m.legacy(&other, 77, false)->index(9) == 2);
        CHECK(m.legacy(&owner, 77, true)->set(1, 3) == Error::None);
        CHECK(m.legacy(&owner, 77, false)->remove(1));
        remaining = 0;
        CHECK(m.clearLegacy(&owner, 77));
        m.shutdown(); remaining = -1;
    }
    CHECK(live == baseline);
}
#ifdef IW_TEST_NATIVE_ALLOCATION
namespace {
const AMX_NATIVE_INFO* table = nullptr;
int tableSize = 0;
cell call(AMX& amx, const char* name, std::initializer_list<cell> arguments) {
    cell parameters[5]{};
    CHECK(arguments.size() < std::size(parameters));
    parameters[0] = static_cast<cell>(arguments.size() * sizeof(cell));
    std::size_t index = 1;
    for (auto value : arguments) parameters[index++] = value;
    for (int i = 0; i < tableSize; ++i)
        if (std::strcmp(name, table[i].name) == 0) return table[i].func(&amx, parameters);
    CHECK(false); return 0;
}
}
int AMXAPI amx_Register(AMX*, const AMX_NATIVE_INFO* entries, int count) {
    table = entries; tableSize = count; return AMX_ERR_NONE;
}
int AMXAPI amx_GetAddr(AMX* amx, cell address, cell** result) {
    *result = reinterpret_cast<cell*>(amx->data + address); return AMX_ERR_NONE;
}
void nativeFailures() {
    for (int operation = 0; operation < 6; ++operation) sweep([=](int budget) {
        Manager m; AMX amx{};
        m.load(&amx); bindNatives(&m); CHECK(registerNatives(&amx) == AMX_ERR_NONE);
        const auto h = m.create(&amx, true, 0);
        CHECK(m.get(&amx, h)->set(1, 2) == Error::None);
        auto invoke = [&] {
            switch (operation) {
                case 0: return call(amx, "Registry_Create", {100, 1});
                case 1: return call(amx, "Registry_Set", {h, 3, 4});
                case 2: return call(amx, "Registry_Set", {h, 1, 4});
                case 3: return call(amx, "SetMapIndex", {77, 3, 4});
                case 4: return call(amx, "Registry_Clear", {h, 1});
                default: return call(amx, "Registry_Reserve", {h, 1000});
            }
        };
        remaining = budget;
        const auto result = invoke(); // No C++ exception may cross the native boundary.
        remaining = -1;
        const bool failed = result == 0;
        if (failed) {
            CHECK(call(amx, "Registry_GetLastError", {}) == static_cast<cell>(Error::Allocation));
            CHECK(m.get(&amx, h)->index(1) == 2);
            CHECK(!m.get(&amx, h)->index(3));
            CHECK(m.get(&amx, h)->size() == 1);
            if (operation == 0) CHECK(!m.get(&amx, h + 1));
            if (operation == 3) CHECK(m.countLegacy(&amx) == 0);
            CHECK(invoke() > 0);
        } else CHECK(call(amx, "Registry_GetLastError", {}) == static_cast<cell>(Error::None));
        auto* r = m.get(&amx, h); CHECK(r && r->invariant());
        if (operation == 0) CHECK(m.get(&amx, h + 1));
        if (operation == 1) CHECK(r->index(3) == 4 && r->id(4) == 3);
        if (operation == 2) CHECK(r->index(1) == 4 && r->id(4) == 1 && !r->id(2));
        if (operation == 3) CHECK(m.legacy(&amx, 77, false)->index(3) == 4);
        if (operation == 4) CHECK(r->size() == 0);
        if (operation == 5) CHECK(r->index(1) == 2 && r->id(2) == 1 && r->size() == 1);
        remaining = 0; m.shutdown(); bindNatives(nullptr); remaining = -1;
        return !failed;
    });
    std::puts("PASS: allocation failures translated at real native entry points (mock AMX), unchanged associations and successful retries");
}
#endif
int main() {
    configureTestDiagnostics();
    loading(false); loading(true);
    creation(false, 0); creation(true, 0);
    creation(false, 100); creation(true, 100);
    compatibility(); emptyLegacyCleanup();
#ifdef IW_TEST_NATIVE_ALLOCATION
    nativeFailures();
#endif
    std::puts("PASS: manager allocation boundaries, owned creation/load/legacy rollback, allocation-free cleanup and balanced allocations");
}
