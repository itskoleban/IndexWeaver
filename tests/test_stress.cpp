#include "../src/mapper_store.hpp"

#include <chrono>
#include <iostream>
#include <vector>
#include <cstdint>

using namespace index_weaver;

int main()
{
	// Define the benchmark scope: 10 Million iterations
	constexpr int ITERATIONS = 10000000;

	std::cout << "[STRESS] Starting stress test with " << ITERATIONS << " iterations...\n";

	IndexRegistryStore store;

	// Lambda function to encapsulate a full benchmark suite for a specific registry category
	auto run_benchmark = [&](const char* name, int registryType)
	{
		std::cout << "--- " << name << " (Registry " << registryType << ") ---\n";

		// Phase 1: Insertion (Set)
		{
			auto start = std::chrono::high_resolution_clock::now();
			
			// Pre-allocate memory to prevent rehashing from skewing the insert timings
			store.reserve(registryType, ITERATIONS);
			
			for (int i = 0; i < ITERATIONS; ++i)
			{
				store.set(registryType, i, i * 2);
			}
			
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = end - start;
			std::cout << "  Insert : " << diff.count() << " s\n";
		}

		// Phase 2: Lookup (Get)
		{
			auto start = std::chrono::high_resolution_clock::now();
			
			// Accumulate the looked up values into a 64-bit integer. 
			// This prevents the compiler from optimizing out the loop via dead code elimination (DCE).
			int64_t sum = 0;
			for (int i = 0; i < ITERATIONS; ++i)
			{
				sum += store.get(registryType, i);
			}
			
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = end - start;
			std::cout << "  Lookup : " << diff.count() << " s (sum verification: " << sum << ")\n";
		}

		// Phase 3: Removal (Remove)
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

	// Benchmark the Fast Path (O(1) Fixed Array lookup + Robin Hood HashMap)
	run_benchmark("Fast Registry (Array)", 1);
	
	// Benchmark the Slow Path (Outer Robin Hood HashMap lookup + Inner Robin Hood HashMap)
	run_benchmark("Slow Registry (HashMap)", 100);

	std::cout << "[PASS] Stress tests completed.\n";

	return 0;
}
