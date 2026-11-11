// tools/rigtest/heap.h - counts heap calls made while a piece of code runs, so a test can prove the audio
// path never allocates. It replaces malloc and friends for the whole program (glibc only), which also
// catches a plugin loaded with dlopen when the program is linked with -rdynamic (rigcheck is).
// Include it in exactly one file of a test program.
#pragma once
#include <atomic>
#include <cstddef>
#include <functional>

#if defined(__GLIBC__) && !defined(__SANITIZE_ADDRESS__)
extern "C" void* __libc_malloc(size_t);
extern "C" void* __libc_calloc(size_t, size_t);
extern "C" void* __libc_realloc(void*, size_t);
extern "C" void* __libc_memalign(size_t, size_t);
extern "C" void __libc_free(void*);
namespace rigtest { inline std::atomic<long> g_heapCalls{0}; inline std::atomic<bool> g_counting{false}; }
static inline void rigtest_note() { if (rigtest::g_counting.load(std::memory_order_relaxed)) rigtest::g_heapCalls++; }
extern "C" void* malloc(size_t n) noexcept { rigtest_note(); return __libc_malloc(n); }
extern "C" void* calloc(size_t a, size_t b) noexcept { rigtest_note(); return __libc_calloc(a, b); }
extern "C" void* realloc(void* p, size_t n) noexcept { rigtest_note(); return __libc_realloc(p, n); }
extern "C" void free(void* p) noexcept { if (p) rigtest_note(); __libc_free(p); }
extern "C" void* aligned_alloc(size_t a, size_t n) noexcept { rigtest_note(); return __libc_memalign(a, n); }
extern "C" void* memalign(size_t a, size_t n) noexcept { rigtest_note(); return __libc_memalign(a, n); }
extern "C" int posix_memalign(void** p, size_t a, size_t n) noexcept { rigtest_note(); *p = __libc_memalign(a, n); return *p ? 0 : 12; }
namespace rigtest {
inline bool heapCounting() { return true; }
inline long heapCallsDuring(const std::function<void()>& f) { g_heapCalls = 0; g_counting = true; f(); g_counting = false; return g_heapCalls.load(); }
}
#else
namespace rigtest {
inline bool heapCounting() { return false; }
inline long heapCallsDuring(const std::function<void()>& f) { f(); return 0; }
}
#endif
