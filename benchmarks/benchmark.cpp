#include "registry.hpp"
#include "manager.hpp"
#include "baseline/mapper_store.hpp"
#include <robin_hood.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
using namespace index_weaver;
using Clock=std::chrono::steady_clock;
struct Tracking : std::pmr::memory_resource {
    std::size_t bytes=0, peak=0;
    void* do_allocate(std::size_t n,std::size_t a) override {
        auto* p=std::pmr::new_delete_resource()->allocate(n,a); bytes+=n; peak=std::max(peak,bytes); return p;
    }
    void do_deallocate(void* p,std::size_t n,std::size_t a) override { bytes-=n; std::pmr::new_delete_resource()->deallocate(p,n,a); }
    bool do_is_equal(const std::pmr::memory_resource& r) const noexcept override {return this==&r;}
};
struct Modern {
    Registry r;
    explicit Modern(bool reverse):r(reverse) {}
    void reserve(int n){r.reserve(static_cast<std::size_t>(n));}
    void set(int k,int v){if(r.set(k,v)!=Error::None) throw std::runtime_error("set failed");}
    int get(int k){return r.index(k).value_or(-1);}
    void remove(int k){if(!r.remove(k)) throw std::runtime_error("remove failed");}
    std::size_t bytes() const{return 0;}
};
struct Original {
    IndexRegistryStore r; int category;
    explicit Original(int type):category(type){}
    void reserve(int n){if(!r.reserve(category,static_cast<std::size_t>(n))) throw std::runtime_error("reserve");}
    void set(int k,int v){if(!r.set(category,k,v)) throw std::runtime_error("set");}
    int get(int k){return r.get(category,k);}
    void remove(int k){if(!r.remove(category,k)) throw std::runtime_error("remove");}
    std::size_t bytes() const{return 0;} // Unknown: robin hood allocates through malloc.
};
template<class Map> struct Raw {
    Map r;
    void reserve(int n){r.reserve(static_cast<std::size_t>(n));}
    void set(int k,int v){r[k]=v;}
    int get(int k){auto it=r.find(k);return it==r.end()?-1:it->second;}
    void remove(int k){if(r.erase(k)!=1) throw std::runtime_error("remove");}
    std::size_t bytes() const{return 0;}
};
std::size_t rss() {
#ifdef __linux__
    std::ifstream file("/proc/self/status"); std::string line;
    while(std::getline(file,line)) if(line.starts_with("VmHWM:")) return std::stoull(line.substr(6))*1024;
#endif
    return 0;
}
template<class F> double measure(F&& operation,std::size_t n) {
    auto start=Clock::now(); operation();
    return std::chrono::duration<double,std::nano>(Clock::now()-start).count()/static_cast<double>(n);
}
template<class Store> void run(Store& r,const std::string& name,int n,int repeat,bool random) {
    std::vector<int> keys(static_cast<std::size_t>(n));
    for(int i=0;i<n;++i) keys[static_cast<std::size_t>(i)]=i*1009;
    if(random){std::mt19937 rng(12345);std::shuffle(keys.begin(),keys.end(),rng);}
    auto record=[&](const char* op,double ns,std::int64_t checksum=0){
        std::cout<<name<<','<<n<<','<<repeat<<','<<(random?"random":"sequential")<<','<<op<<','<<ns<<','<<checksum<<','<<r.bytes()<<','<<rss()<<'\n';
    };
    record("reserve",measure([&]{r.reserve(n);},1));
    record("insert",measure([&]{for(int k:keys) r.set(k,k/1009);},keys.size()));
    std::int64_t sum=0;
    // Repeat tiny datasets enough to keep clock quantization from dominating lookups.
    const int rounds=std::max(1,100000/n);
    auto hit=measure([&]{for(int j=0;j<rounds;++j) for(int k:keys) sum+=r.get(k);},keys.size()*static_cast<std::size_t>(rounds));
    if(sum!=static_cast<std::int64_t>(n)*(n-1)/2*rounds) throw std::runtime_error("hit checksum");
    record("hit",hit,sum); sum=0;
    auto miss=measure([&]{for(int j=0;j<rounds;++j) for(int k:keys) sum+=r.get(-k-1);},keys.size()*static_cast<std::size_t>(rounds));
    if(sum!=-static_cast<std::int64_t>(n)*rounds) throw std::runtime_error("miss checksum");
    record("miss",miss,sum);
    // Unique replacement values ensure the exact workload is valid in bidirectional mode.
    record("update",measure([&]{for(int k:keys) r.set(k,k/1009+n);},keys.size()));
    record("remove",measure([&]{for(int k:keys) r.remove(k);},keys.size()));
    if(name!="v2") return;
    Manager m; int owner=0; m.load(&owner); std::int64_t handles=0;
    auto creation=measure([&]{for(int i=0;i<10000;++i){auto h=m.create(&owner,false,0);handles+=h;if(!m.destroy(&owner,h))throw std::runtime_error("destroy");}},10000); record("create_destroy",creation,handles);
}
int main(int argc,char** argv) {
    if(argc!=5){std::cerr<<"usage: iw_benchmark v2|v2reverse|v1fast|v1slow|robin|std N repeat random(0/1)\n";return 2;}
    std::string name=argv[1];int n=std::stoi(argv[2]),repeat=std::stoi(argv[3]);bool random=std::stoi(argv[4])!=0;
    if(n<=0 || n>1000000)return 2;
    if(name=="v2memory" || name=="v2reversememory") {
        Tracking memory; Registry registry(name=="v2reversememory",&memory);
        registry.reserve(static_cast<std::size_t>(n));
        for(int i=0;i<n;++i) if(registry.set(i*1009,i)!=Error::None) return 1;
        std::cout<<name<<','<<n<<','<<memory.bytes<<','<<memory.peak<<','<<rss()<<'\n'; return 0;
    }
    if(name=="v2" || name=="v2reverse"){Modern r(name=="v2reverse");run(r,name,n,repeat,random);}
    else if(name=="v1fast" || name=="v1slow"){Original r(name=="v1fast"?1:100);run(r,name,n,repeat,random);}
    else if(name=="robin"){Raw<robin_hood::unordered_flat_map<int,int>> r;run(r,name,n,repeat,random);}
    else if(name=="std"){Raw<std::unordered_map<int,int,Hash>> r;run(r,name,n,repeat,random);}
    else return 2;
}
