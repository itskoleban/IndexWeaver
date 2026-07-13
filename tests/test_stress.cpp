#include "../src/mapper_store.hpp"

#include <chrono>
#include <iostream>
#include <vector>
#include <cstdint>

using namespace index_weaver;

int main()
{
	constexpr int ITERATIONS = 10000000;

	std::cout << "[STRESS] Starting stress test with " << ITERATIONS << " iterations...\n";

	IndexRegistryStore store;

	auto run_benchmark = [&](const char* name, int registryType)
	{
		std::cout << "--- " << name << " (Registry " << registryType << ") ---\n";

		// Insert
		{
			auto start = std::chrono::high_resolution_clock::now();
			store.reserve(registryType, ITERATIONS);
			for (int i = 0; i < ITERATIONS; ++i)
			{
				store.set(registryType, i, i * 2);
			}
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = end - start;
			std::cout << "  Insert : " << diff.count() << " s\n";
		}

		// Lookup
		{
			auto start = std::chrono::high_resolution_clock::now();
			int64_t sum = 0;
			for (int i = 0; i < ITERATIONS; ++i)
			{
				sum += store.get(registryType, i);
			}
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = end - start;
			std::cout << "  Lookup : " << diff.count() << " s (sum verification: " << sum << ")\n";
		}

		// Remove
		{
			auto start = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < ITERATIONS; ++i)
			{
				store.remove(registryType, i);
			}
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = end - start;
			std::cout << "  Remove : " << diff.count() << " s\n";
		}
		std::cout << "\n";
	};

	run_benchmark("Fast Registry (Array)", 1);
	run_benchmark("Slow Registry (HashMap)", 100);

	std::cout << "[PASS] Stress tests completed.\n";

	return 0;
}
