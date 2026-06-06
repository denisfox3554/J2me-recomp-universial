#include "gc.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// GC — malloc-backed, zeroed allocations.
// J2ME games have tiny heaps (128–512 KB), so a simple allocator is fine.
// Replace with a pool allocator if profiling shows malloc overhead.
// ─────────────────────────────────────────────────────────────────────────────

void* j2me_alloc(size_t size) {
    void* p = std::calloc(1, size);
    if (!p) {
        std::fputs("[gc] Out of memory\n", stderr);
        std::abort();
    }
    return p;
}

void j2me_free(void* ptr) {
    std::free(ptr);
}

void j2me_array_oob(int idx, int len) {
    std::fprintf(stderr,
        "[gc] ArrayIndexOutOfBoundsException: index %d, length %d\n", idx, len);
    std::abort();
}

void j2me_null_deref() {
    std::fputs("[gc] NullPointerException\n", stderr);
    std::abort();
}

void j2me_throw_stub(const char* exc_name, const char* msg) {
    std::fprintf(stderr, "[gc] %s: %s\n",
        exc_name ? exc_name : "Exception",
        msg      ? msg      : "");
    std::abort();
}
