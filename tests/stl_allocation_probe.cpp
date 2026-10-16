// Standalone upstream-library reproducer: no IndexWeaver engine is linked.
#include "diagnostics.hpp"
#include <cstdlib>
#include <memory_resource>
#include <unordered_map>

class FailingResource final : public std::pmr::memory_resource {
    int remaining_;
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        if (remaining_-- == 0) throw std::bad_alloc();
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* memory, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(memory, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
public:
    explicit FailingResource(int budget) : remaining_(budget) {}
};

int main(int argc, char** argv) {
    configureTestDiagnostics();
    FailingResource memory(argc == 2 ? std::atoi(argv[1]) : 2);
    try {
        std::pmr::unordered_map<int, int> map(&memory);
        std::puts("Empty map construction succeeded");
    } catch (const std::bad_alloc&) {
        std::puts("Allocation failure caught by caller");
    }
}
