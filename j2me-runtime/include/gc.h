#pragma once
#include <cstddef>
#include <cstdint>

// ── Allocator ─────────────────────────────────────────────────────────────────
void* j2me_alloc(size_t size);
void  j2me_free(void* ptr);

// ── Error handlers — declared BEFORE templates that call them ─────────────────
void j2me_array_oob(int idx, int len);
void j2me_null_deref();
void j2me_throw_stub(const char* exc_name, const char* msg);

// ── Array helpers ─────────────────────────────────────────────────────────────
template<typename T>
T* j2me_new_array(int length) {
    size_t sz = sizeof(int) + sizeof(T) * static_cast<size_t>(length < 0 ? 0 : length);
    auto* buf = static_cast<uint8_t*>(j2me_alloc(sz));
    *reinterpret_cast<int*>(buf) = length;
    return reinterpret_cast<T*>(buf + sizeof(int));
}

template<typename T>
int j2me_array_length(T* arr) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(arr) - sizeof(int));
}

template<typename T>
T& j2me_array_get(T* arr, int idx) {
#ifndef NDEBUG
    int len = j2me_array_length(arr);
    if (idx < 0 || idx >= len) j2me_array_oob(idx, len);
#endif
    return arr[idx];
}
