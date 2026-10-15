#include "manager.hpp"
#include "baseline/mapper_store.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace index_weaver;
using Clock = std::chrono::steady_clock;
struct ModernMany {
    Manager manager;
    int owner = 0;
    std::vector<Handle> handles;
    ModernMany(int registries, int size, bool reverse) {
        manager.load(&owner); handles.reserve(static_cast<std::size_t>(registries));
        for (int i = 0; i < registries; ++i) {
            auto h = manager.create(&owner, reverse, static_cast<std::size_t>(size));
            if (!h) throw std::runtime_error("create");
            handles.push_back(h);
        }
    }
    Registry& registry(int category) { return *manager.get(&owner, handles[static_cast<std::size_t>(category)]); }
    void set(int category, int key, int index) {
        if (registry(category).set(key, index) != Error::None) throw std::runtime_error("set");
    }
    int get(int category, int key) { return registry(category).index(key).value_or(-1); }
    void remove(int category, int key) { if (!registry(category).remove(key)) throw std::runtime_error("remove"); }
};
struct OriginalMany {
    IndexRegistryStore store;
    OriginalMany(int registries, int size, bool) {
        for (int i = 0; i < registries; ++i)
            if (!store.reserve(i, static_cast<std::size_t>(size))) throw std::runtime_error("reserve");
    }
    void set(int category, int key, int index) { if (!store.set(category, key, index)) throw std::runtime_error("set"); }
    int get(int category, int key) { return store.get(category, key); }
    void remove(int category, int key) { if (!store.remove(category, key)) throw std::runtime_error("remove"); }
};
std::uint64_t rss() {
#ifdef __linux__
    std::ifstream file("/proc/self/status"); std::string line;
    while (std::getline(file, line)) if (line.starts_with("VmHWM:")) return std::stoull(line.substr(6)) * 1024;
#endif
    return 0;
}
template<class Store> void run(const std::string& name, int registries, int size, int repeat) {
    std::vector<std::pair<int, int>> workload;
    const auto count = static_cast<std::size_t>(registries) * static_cast<std::size_t>(size);
    workload.reserve(count);
    for (int i = 0; i < registries; ++i) for (int j = 0; j < size; ++j) workload.emplace_back(i, j);
    std::mt19937 random(12345); std::shuffle(workload.begin(), workload.end(), random);
    auto record = [&](const char* operation, double elapsed, std::int64_t checksum = 0) {
        std::cout << name << ',' << registries << ',' << size << ',' << repeat << ',' << operation
                  << ',' << elapsed << ',' << checksum << ',' << rss() << '\n';
    };
    auto measure = [](auto&& operation, std::size_t operations) {
        const auto start = Clock::now(); operation();
        return std::chrono::duration<double, std::nano>(Clock::now() - start).count() / static_cast<double>(operations);
    };
    std::unique_ptr<Store> store;
    record("create_reserve", measure([&] { store = std::make_unique<Store>(registries, size, name == "v2reverse"); }, static_cast<std::size_t>(registries)));
    record("insert", measure([&] { for (auto [i, j] : workload) store->set(i, j * 1009, j); }, count));
    const auto rounds = std::max<std::size_t>(1, 100000 / count);
    std::int64_t sum = 0;
    auto hits = measure([&] { for (std::size_t k = 0; k < rounds; ++k) for (auto [i, j] : workload) sum += store->get(i, j * 1009); }, count * rounds);
    const auto expected = static_cast<std::int64_t>(registries) * size * (size - 1) / 2 * static_cast<std::int64_t>(rounds);
    if (sum != expected) throw std::runtime_error("hit checksum");
    record("hit", hits, sum); sum = 0;
    auto misses = measure([&] { for (std::size_t k = 0; k < rounds; ++k) for (auto [i, j] : workload) sum += store->get(i, -j * 1009 - 1); }, count * rounds);
    if (sum != -static_cast<std::int64_t>(count * rounds)) throw std::runtime_error("miss checksum");
    record("miss", misses, sum);
    record("update", measure([&] { for (auto [i, j] : workload) store->set(i, j * 1009, j + size); }, count));
    sum = 0;
    for (auto [i, j] : workload) sum += store->get(i, j * 1009);
    if (sum != expected / static_cast<std::int64_t>(rounds) + static_cast<std::int64_t>(count) * size)
        throw std::runtime_error("update checksum");
    record("remove", measure([&] { for (auto [i, j] : workload) store->remove(i, j * 1009); }, count));
    for (auto [i, j] : workload) if (store->get(i, j * 1009) != -1) throw std::runtime_error("remove verification");
    record("destroy", measure([&] { store.reset(); }, static_cast<std::size_t>(registries)));
}
int main(int argc, char** argv) {
    if (argc != 5) return 2;
    const std::string name = argv[1];
    const int registries = std::stoi(argv[2]), size = std::stoi(argv[3]), repeat = std::stoi(argv[4]);
    if (registries <= 0 || registries > 10000 || size <= 0 || size > 100) return 2;
    if (name == "v1") run<OriginalMany>(name, registries, size, repeat);
    else if (name == "v2" || name == "v2reverse") run<ModernMany>(name, registries, size, repeat);
    else return 2;
}
