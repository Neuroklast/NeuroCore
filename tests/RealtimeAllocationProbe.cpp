#include <cstddef>
namespace { thread_local bool active = false; thread_local unsigned count = 0; }
extern "C" void nkBeginAllocationProbe() { count = 0; active = true; }
extern "C" unsigned nkEndAllocationProbe() { active = false; return count; }
extern "C" void* __real_malloc(std::size_t);
extern "C" void* __real_calloc(std::size_t, std::size_t);
extern "C" void* __real_realloc(void*, std::size_t);
extern "C" void* __wrap_malloc(std::size_t n) { if (active) ++count; return __real_malloc(n); }
extern "C" void* __wrap_calloc(std::size_t n, std::size_t s) { if (active) ++count; return __real_calloc(n,s); }
extern "C" void* __wrap_realloc(void* p, std::size_t n) { if (active) ++count; return __real_realloc(p,n); }
#include <new>
#include <cstdlib>
void* operator new(std::size_t n)
{
    if (active) ++count;
    if (auto* p = __real_malloc(n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
