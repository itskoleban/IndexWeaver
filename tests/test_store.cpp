#include "../src/mapper_store.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>

using namespace index_weaver;

static void run_unit_tests()
{
	IndexRegistryStore store;

	std::cout << "[TEST] 1. Set / Get / Has / Remove...\n";
	assert(store.set(1, 100, 5));
	assert(store.has(1, 100));
	assert(store.get(1, 100) == 5);
	assert(store.remove(1, 100));
	assert(!store.has(1, 100));
	assert(store.get(1, 100) == IndexRegistryStore::INVALID_INDEX);
	assert(store.totalRegistries() == 0);

	std::cout << "[TEST] 2. Registry lifecycle...\n";
	assert(store.set(10, 1, 100));
	assert(store.set(10, 2, 200));
	assert(store.set(20, 1, 300));
	assert(store.totalRegistries() == 2);
	assert(store.count(10) == 2);
	assert(store.count(20) == 1);

	assert(store.clearType(10));
	assert(!store.clearType(10));
	assert(store.totalRegistries() == 1);
	assert(store.count(10) == 0);

	store.clearAll();
	assert(store.totalRegistries() == 0);

	std::cout << "[TEST] 3. Reserve...\n";
	assert(store.reserve(5, 50000));
	assert(store.totalRegistries() == 1);
	assert(store.count(5) == 0);
	assert(!store.reserve(-1, 50000));
	assert(!store.reserve(5, 0));
	assert(!store.reserve(5, IndexRegistryStore::MAX_RESERVED_CAPACITY + 1));

	std::cout << "[TEST] 4. Key overwrite...\n";
	assert(store.set(1, 500, 10));
	assert(store.set(1, 500, 99));
	assert(store.get(1, 500) == 99);
	assert(store.count(1) == 1);

	std::cout << "[TEST] 5. Invalid parameters...\n";
	assert(!store.set(-5, 10, 1));
	assert(!store.set(1, 10, -5));
	assert(store.get(-1, 10) == IndexRegistryStore::INVALID_INDEX);
	assert(!store.remove(-1, 10));
	assert(!store.clearType(-1));

	std::cout << "[TEST] 6. Empty-registry cleanup...\n";
	assert(store.set(77, 1, 1));
	assert(store.set(77, 2, 2));
	assert(store.remove(77, 1));
	assert(store.totalRegistries() == 3); // registries 1, 5, 77
	assert(store.remove(77, 2));
	assert(store.totalRegistries() == 2); // registries 1, 5
	assert(store.count(77) == 0);

	std::cout << "\n[PASS] All unit tests passed successfully.\n\n";
}

int main()
{
	run_unit_tests();
	return 0;
}
