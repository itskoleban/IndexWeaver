#include "../src/mapper_store.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>

using namespace index_weaver;

void run_unit_tests()
{
    IndexRegistryStore store;

    std::cout << "[TEST] 1. Set / Get / Has / Remove...\n";
    assert(store.set(1, 100, 5));
    assert(store.has(1, 100));
    assert(store.get(1, 100) == 5);
    assert(store.remove(1, 100));
    assert(!store.has(1, 100));
    assert(store.get(1, 100) == IndexRegistryStore::INVALID_INDEX);

    std::cout << "[TEST] 2. Registry lifecycle...\n";
    store.set(10, 1, 100);
    store.set(10, 2, 200);
    store.set(20, 1, 300);

    assert(store.totalRegistries() == 2);

    assert(store.clearType(10));
    assert(store.totalRegistries() == 1);

    store.clearAll();

    assert(store.totalRegistries() == 0);

    std::cout << "[TEST] 3. Reserve...\n";
    assert(store.reserve(5, 50000));
    assert(!store.reserve(-1, 50000));
    assert(!store.reserve(5, 0));

    std::cout << "[TEST] 4. Key overwrite...\n";
    store.set(1, 500, 10);
    store.set(1, 500, 99);

    assert(store.get(1, 500) == 99);
    assert(store.count(1) == 1);

    std::cout << "[TEST] 5. Invalid parameters...\n";
    assert(!store.set(-5, 10, 1));
    assert(!store.set(1, 10, -5));
    assert(store.get(-1, 10) == IndexRegistryStore::INVALID_INDEX);
    assert(!store.remove(-1, 10));

    std::cout << "\n[PASS] All unit tests passed successfully.\n\n";
}

void benchmark(std::size_t N)
{
    using namespace std::chrono;

    IndexRegistryStore store;
    const int TYPE = 99;

    std::cout << "========== BENCHMARK (" << N << " records) ==========\n";

    store.reserve(TYPE, N);

    // ----------------------------
    // Insertion benchmark
    // ----------------------------

    auto start = high_resolution_clock::now();

    for (std::size_t i = 0; i < N; ++i)
        store.set(TYPE, static_cast<int>(i), static_cast<int>(i));

    auto end = high_resolution_clock::now();

    auto insert_us = duration_cast<microseconds>(end - start).count();
    double insert_sec = duration<double>(end - start).count();

    std::cout
        << "Insertion : "
        << insert_us
        << " us ("
        << static_cast<long long>(N / insert_sec)
        << " ops/sec)\n";

    // ----------------------------
    // Lookup benchmark
    // ----------------------------

    volatile std::uint64_t checksum = 0;

    start = high_resolution_clock::now();

    for (std::size_t i = 0; i < N; ++i)
        checksum += static_cast<std::uint64_t>(
            store.get(TYPE, static_cast<int>(i)));

    end = high_resolution_clock::now();

    auto lookup_us = duration_cast<microseconds>(end - start).count();
    double lookup_sec = duration<double>(end - start).count();

    std::cout
        << "Lookup    : "
        << lookup_us
        << " us ("
        << static_cast<long long>(N / lookup_sec)
        << " ops/sec)\n";

    // ----------------------------
    // Clear benchmark
    // ----------------------------

    start = high_resolution_clock::now();

    store.clearType(TYPE);

    end = high_resolution_clock::now();

    auto clear_us = duration_cast<microseconds>(end - start).count();
    double clear_sec = duration<double>(end - start).count();

    std::cout
        << "Clear     : "
        << clear_us
        << " us ("
        << static_cast<long long>(1.0 / clear_sec)
        << " clears/sec)\n";

    std::cout << "Checksum  : " << checksum << '\n';
    std::cout << "=====================================================\n\n";
}

int main()
{
    run_unit_tests();

    benchmark(100000);
    benchmark(500000);
    benchmark(1000000);
    benchmark(5000000);
    benchmark(10000000);

    return 0;
}