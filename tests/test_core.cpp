#include "manager.hpp"
#include "diagnostics.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string_view>
#include <vector>
using namespace index_weaver;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __FILE__ << ':' << __LINE__ << ": " << #__VA_ARGS__ << '\n'; std::exit(1); } } while(false)
struct Memory : std::pmr::memory_resource {
    int untilFailure = -1;
    std::size_t bytes = 0;
    void* do_allocate(std::size_t n, std::size_t alignment) override {
        if (untilFailure == 0) throw std::bad_alloc();
        if (untilFailure > 0) --untilFailure;
        auto* p = std::pmr::new_delete_resource()->allocate(n, alignment); bytes += n; return p;
    }
    void do_deallocate(void* p, std::size_t n, std::size_t alignment) override {
        bytes -= n; std::pmr::new_delete_resource()->deallocate(p,n,alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
};
void functional() {
    Registry f;
    CHECK(!f.index(0)); CHECK(f.set(-2147483647-1,0) == Error::None);
    CHECK(f.set(2147483647,2147483647) == Error::None);
    CHECK(f.set(1,-1) == Error::InvalidArgument);
    CHECK(f.set(10,0) == Error::None); // forward-only aliases allowed
    CHECK(f.set(10,4,true) == Error::DuplicateID); CHECK(f.index(10) == 0);
    CHECK(f.set(10,4) == Error::None); CHECK(f.index(10) == 4);
    CHECK(!f.id(4)); CHECK(f.remove(10)); CHECK(!f.remove(10));
    f.reserve(100); f.clear(); CHECK(f.size()==0); f.clear(true);
    Registry b(true);
    CHECK(b.set(-1,2) == Error::None); CHECK(b.id(2) == -1);
    CHECK(b.set(7,2) == Error::OccupiedIndex); CHECK(!b.index(7));
    CHECK(b.set(-1,2) == Error::None); CHECK(b.set(-1,2,true) == Error::DuplicateID);
    CHECK(b.set(-1,3) == Error::None); CHECK(!b.id(2)); CHECK(b.id(3) == -1);
    CHECK(b.remove(-1)); CHECK(!b.id(3)); CHECK(b.invariant());
    bool threw = false;
    try { b.reserve(static_cast<std::size_t>(2147483647) + 1); } catch (const std::length_error&) { threw = true; }
    CHECK(threw);
}
void allocation() {
    for (int budget=0; budget<12; ++budget) {
        std::cout << "[allocation] budget=" << budget << '\n' << std::flush;
        Memory m;
        { Registry b(true,&m); const auto emptyBytes=m.bytes; b.reserve(2); CHECK(b.set(1,1) == Error::None); CHECK(b.set(2,2) == Error::None);
          m.untilFailure=budget;
          try { CHECK(b.set(3,3) == Error::None); } catch (const std::bad_alloc&) { CHECK(!b.index(3)); CHECK(!b.id(3)); }
          CHECK(b.invariant()); CHECK(b.index(1) == 1);
          m.untilFailure=budget;
          try { CHECK(b.set(1,4) == Error::None); } catch (const std::bad_alloc&) { CHECK(b.index(1) == 1); }
          CHECK(b.invariant());
          m.untilFailure=budget;
          auto count=b.size();
          try { b.reserve(500); } catch (const std::bad_alloc&) { CHECK(b.size()==count); }
          CHECK(b.invariant()); m.untilFailure=budget;
          count=b.size();
          try { b.clear(true); CHECK(b.size()==0); }
          catch (const std::bad_alloc&) { CHECK(b.size()==count); }
          CHECK(b.invariant()); m.untilFailure=-1;
          b.clear(true); CHECK(m.bytes==emptyBytes);
        } CHECK(m.bytes==0);
    }
}
void lifecycle() {
    Manager m; int a=0,b=0; m.load(&a); m.load(&b);
    auto h=m.create(&a,true,100); CHECK(h>0); auto* r=m.get(&a,h); CHECK(r);
    CHECK(r->set(1,2) == Error::None); CHECK(!m.get(&b,h)); CHECK(m.error(&b)==Error::WrongOwner);
    CHECK(!m.destroy(&b,h)); CHECK(m.destroy(&a,h)); CHECK(!m.destroy(&a,h));
    auto h2=m.create(&a,false,0); CHECK(h2>h); CHECK(!m.get(&a,h));
    CHECK(m.legacy(&a,77,true)->set(1,3)==Error::None);
    CHECK(!m.legacy(&b,77,false)); CHECK(m.countLegacy(&a)==1);
    m.unload(&a); CHECK(!m.get(&a,h2)); m.load(&a); auto h3=m.create(&a,false,0); CHECK(h3>h2);
    m.reset(); CHECK(!m.get(&a,h3)); CHECK(m.loaded(&a)); CHECK(m.countLegacy(&a)==0);
    for(int i=0;i<10000;++i) { auto x=m.create(&a,false,0); CHECK(x>h3); CHECK(m.destroy(&a,x)); }
    m.load(&a); CHECK(!m.get(&a,h3));
    std::vector<Handle> handles;
    for(int i=0;i<1000;++i) {
        auto h4=m.create(&a,i%2==0,5); handles.push_back(h4);
        for(int j=0;j<5;++j) CHECK(m.get(&a,h4)->set(j,j)==Error::None);
    }
    for(auto handle:handles) CHECK(m.get(&a,handle)->size()==5);
    m.unload(&a); m.unload(&a); m.reset(); m.shutdown(); m.shutdown();
    CHECK(!m.loaded(&a)); m.load(&a); CHECK(m.create(&a,false,0)>handles.back());
}
void randomized(bool reverse, int iterations, unsigned keySpace) {
    Registry registry(reverse); std::map<Value,Value> reference;
    std::mt19937 rng(0x152060U);
    for(int n=0;n<iterations;++n) {
        auto key=static_cast<Value>(rng()%keySpace)-static_cast<Value>(keySpace/2);
        auto value=static_cast<Value>(rng()%keySpace);
        auto op=rng()%8;
        if(op<4) {
            const bool only=op==0;
            Error expected=Error::None;
            auto it=reference.find(key);
            if(only && it!=reference.end()) expected=Error::DuplicateID;
            else if(reverse) for(const auto& [k,v]:reference) if(k!=key && v==value) expected=Error::OccupiedIndex;
            CHECK(registry.set(key,value,only)==expected);
            if(expected==Error::None) reference[key]=value;
        } else if(op==4) CHECK(registry.remove(key)==(reference.erase(key)!=0));
        else if(op==5) {
            const auto it=reference.find(key);
            CHECK(registry.index(key)==(it==reference.end()?std::nullopt:std::optional<Value>(it->second)));
        } else if(op==6 && n%137==0) { registry.clear(n%2==0); reference.clear(); }
        else if(op==7) registry.reserve(reference.size()+10);
        CHECK(registry.size()==reference.size()); CHECK(registry.invariant());
        if(n%1000==0) for(const auto& [k,v]:reference) { CHECK(registry.index(k)==v); if(reverse) CHECK(registry.id(v)==k); }
    }
}
void stress() {
    Registry r(true); constexpr int count=1000000; r.reserve(count);
    for(int i=0;i<count;++i) CHECK(r.set(i*1009,i)==Error::None);
    CHECK(r.invariant());
    for(int i=count-1;i>=0;--i) { CHECK(r.index(i*1009)==i); CHECK(r.id(i)==i*1009); CHECK(r.remove(i*1009)); }
    CHECK(r.size()==0); CHECK(r.invariant());
}
void churn(bool bidirectional) {
    // Bounded independent array model: -1 denotes an absent mapping because
    // indices are nonnegative. Sparse signed keys remain within the cell range.
    constexpr int slots = 32768, operations = 5000000;
    Memory memory;
    {
        Registry r(bidirectional, &memory);
        std::vector<Value> forward(slots, -1), reverse(slots * 2, -1);
        std::mt19937 random(0x20261015U);
        std::size_t count = 0;
        auto key = [](int slot) -> Value { return slot * 65537 - 1073741824; };
        auto verify = [&] {
            CHECK(r.invariant()); CHECK(r.size() == count);
            for (int slot = 0; slot < slots; ++slot) {
                auto expected = forward[static_cast<std::size_t>(slot)];
                CHECK(r.index(key(slot)) == (expected < 0 ? std::nullopt : std::optional<Value>(expected)));
            }
            if (bidirectional) for (int index = 0; index < slots * 2; ++index) {
                auto slot = reverse[static_cast<std::size_t>(index)];
                CHECK(r.id(index) == (slot < 0 ? std::nullopt : std::optional<Value>(key(slot))));
            }
        };
        for (int step = 0; step < operations; ++step) {
            const auto slot = static_cast<int>(random() % slots);
            const auto value = static_cast<int>(random() % (slots * 2));
            const auto operation = random() % 10;
            auto& old = forward[static_cast<std::size_t>(slot)];
            if (operation < 5) {
                const bool insertOnly = operation == 0;
                Error expected = Error::None;
                if (insertOnly && old >= 0) expected = Error::DuplicateID;
                else if (bidirectional && reverse[static_cast<std::size_t>(value)] >= 0 &&
                    reverse[static_cast<std::size_t>(value)] != slot) expected = Error::OccupiedIndex;
                CHECK(r.set(key(slot), value, insertOnly) == expected);
                if (expected == Error::None) {
                    if (old < 0) ++count;
                    else if (bidirectional) reverse[static_cast<std::size_t>(old)] = -1;
                    old = value;
                    if (bidirectional) reverse[static_cast<std::size_t>(value)] = slot;
                }
            } else if (operation < 8) {
                CHECK(r.remove(key(slot)) == (old >= 0));
                if (old >= 0) {
                    if (bidirectional) reverse[static_cast<std::size_t>(old)] = -1;
                    old = -1; --count;
                }
            } else {
                CHECK(r.index(key(slot)) == (old < 0 ? std::nullopt : std::optional<Value>(old)));
                if (bidirectional) {
                    const auto owner = reverse[static_cast<std::size_t>(value)];
                    CHECK(r.id(value) == (owner < 0 ? std::nullopt : std::optional<Value>(key(owner))));
                }
            }
            CHECK(r.size() == count);
            if (step % 4096 == 0) CHECK(r.invariant());
            if ((step + 1) % 1000000 == 0) {
                verify();
                r.clear((step + 1) % 2000000 == 0);
                std::fill(forward.begin(), forward.end(), -1);
                std::fill(reverse.begin(), reverse.end(), -1);
                count = 0; r.reserve(slots);
            }
        }
        verify();
    }
    CHECK(memory.bytes == 0);
}
int main(int argc, char** argv) {
    configureTestDiagnostics();
    if (argc == 2 && std::string_view(argv[1]) == "churn") {
        churn(false); churn(true);
        std::cout << "PASS: 10 million mixed sparse signed-ID operations, both modes, independent forward/reverse models and balanced PMR allocations\n";
    } else if (argc == 2 && std::string_view(argv[1]) == "stress") {
        stress(); std::cout << "PASS: million sparse bidirectional mappings\n";
    } else if (argc == 1) {
        std::cout << "[core] functional\n" << std::flush; functional();
        std::cout << "[core] allocation rollback\n" << std::flush; allocation();
        std::cout << "[core] ownership and lifecycle\n" << std::flush; lifecycle();
        for(bool reverse : {false,true}) {
            std::cout << "[core] reference model reverse=" << reverse << " small\n" << std::flush;
            randomized(reverse,100000,65);
            std::cout << "[core] reference model reverse=" << reverse << " medium\n" << std::flush;
            randomized(reverse,10000,401);
        }
        std::cout << "PASS: functional, allocation rollback, lifecycle, 220000 randomized operations\n";
    } else return 2;
}
