#include "../src/mapper_store.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>

using namespace index_weaver;

static void run_unit_tests()
{
	// Initialize a fresh instance of our hybrid storage engine
	IndexRegistryStore store;

	std::cout << "[TEST] 1. Set / Get / Has / Remove...\n";
	
	// Insert mapping: Registry Type 1, ID 100 -> Index 5
	assert(store.set(1, 100, 5));
	
	// Verify the ID was properly registered
	assert(store.has(1, 100));
	
	// Retrieve the mapped index and assert it matches our insertion
	assert(store.get(1, 100) == 5);
	
	// Remove the mapping and verify it returns true
	assert(store.remove(1, 100));
	
	// Verify it no longer exists in memory
	assert(!store.has(1, 100));
	assert(store.get(1, 100) == IndexRegistryStore::INVALID_INDEX);
	
	// Since registry 1 is a fast-path array, it should not increment the active total registries count 
	// unless it contains elements (it's currently empty).
	assert(store.totalRegistries() == 0);

	std::cout << "[TEST] 2. Registry lifecycle...\n";
	
	// Populate two different registries
	assert(store.set(10, 1, 100));
	assert(store.set(10, 2, 200));
	assert(store.set(20, 1, 300));
	
	// Ensure the total count of active registries is 2 (Types 10 and 20)
	assert(store.totalRegistries() == 2);
	
	// Ensure element counts match our insertions
	assert(store.count(10) == 2);
	assert(store.count(20) == 1);

	// Completely clear Registry Type 10
	assert(store.clearType(10));
	
	// Attempting to clear an already empty/cleared registry should return false
	assert(!store.clearType(10));
	
	// Ensure Registry 20 is the only one remaining
	assert(store.totalRegistries() == 1);
	assert(store.count(10) == 0);

	// Perform a hard reset
	store.clearAll();
	assert(store.totalRegistries() == 0);

	std::cout << "[TEST] 3. Reserve...\n";
	
	// Pre-allocate space for 50,000 elements in Registry Type 5
	assert(store.reserve(5, 50000));
	
	// Reserving capacity shouldn't artificially inflate the registry/element counts
	assert(store.totalRegistries() == 0);
	assert(store.count(5) == 0);
	
	// Assert bounds validation works correctly
	assert(!store.reserve(-1, 50000));
	assert(!store.reserve(5, 0));
	assert(!store.reserve(5, IndexRegistryStore::MAX_RESERVED_CAPACITY + 1));

	std::cout << "[TEST] 4. Key overwrite...\n";
	assert(store.set(1, 500, 10)); // Initial mapping
	assert(store.set(1, 500, 99)); // Overwrite with new index
	
	// Ensure the index was updated, not duplicated
	assert(store.get(1, 500) == 99);
	assert(store.count(1) == 1);

	std::cout << "[TEST] 5. Invalid parameters...\n";
	assert(!store.set(-5, 10, 1)); // Invalid registry category
	assert(!store.set(1, 10, -5)); // Invalid array index
	assert(store.get(-1, 10) == IndexRegistryStore::INVALID_INDEX);
	assert(!store.remove(-1, 10));
	assert(!store.clearType(-1));

	std::cout << "[TEST] 6. Empty-registry cleanup...\n";
	// Note: Registry 77 falls into the Slow Path (Dynamic Map) because it is >= 64.
	assert(store.set(77, 1, 1));
	assert(store.set(77, 2, 2));
	
	// Remove first element
	assert(store.remove(77, 1));
	assert(store.totalRegistries() == 2); // Registry 1 (from Test 4) and Registry 77
	
	// Removing the last element in a dynamic registry should trigger auto-destruction
	assert(store.remove(77, 2));
	assert(store.totalRegistries() == 1); // Only Registry 1 remains
	assert(store.count(77) == 0);

	std::cout << "\n[PASS] All unit tests passed successfully.\n\n";
}

int main()
{
	run_unit_tests();
	return 0;
}
