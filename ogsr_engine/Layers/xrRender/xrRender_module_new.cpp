#include "stdafx.h"

// Рендер собирается отдельной DLL и грузится в xrEngine.exe, где xrCore слинкован статически и
// глобально перегружает operator new/delete (mimalloc). Чтобы память, выделенная через new в рендере,
// корректно освобождалась через delete в движке/игре (и наоборот), все операторы new/delete этого модуля
// перенаправляются в аллокатор exe. Реализация функций - в xrCore/xrMemory.cpp

#ifndef XRRENDER_STATIC

#include <new>

XRCORE_API void* xr_module_new(size_t n);
XRCORE_API void* xr_module_new_nothrow(size_t n) noexcept;
XRCORE_API void* xr_module_new_aligned(size_t n, size_t al);
XRCORE_API void* xr_module_new_aligned_nothrow(size_t n, size_t al) noexcept;
XRCORE_API void xr_module_delete(void* p) noexcept;
XRCORE_API void xr_module_delete_aligned(void* p, size_t al) noexcept;

void* operator new(std::size_t n) { return xr_module_new(n); }
void* operator new[](std::size_t n) { return xr_module_new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return xr_module_new_nothrow(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return xr_module_new_nothrow(n); }

void* operator new(std::size_t n, std::align_val_t al) { return xr_module_new_aligned(n, static_cast<size_t>(al)); }
void* operator new[](std::size_t n, std::align_val_t al) { return xr_module_new_aligned(n, static_cast<size_t>(al)); }
void* operator new(std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept { return xr_module_new_aligned_nothrow(n, static_cast<size_t>(al)); }
void* operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept { return xr_module_new_aligned_nothrow(n, static_cast<size_t>(al)); }

void operator delete(void* p) noexcept { xr_module_delete(p); }
void operator delete[](void* p) noexcept { xr_module_delete(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { xr_module_delete(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { xr_module_delete(p); }
void operator delete(void* p, std::size_t) noexcept { xr_module_delete(p); }
void operator delete[](void* p, std::size_t) noexcept { xr_module_delete(p); }

void operator delete(void* p, std::align_val_t al) noexcept { xr_module_delete_aligned(p, static_cast<size_t>(al)); }
void operator delete[](void* p, std::align_val_t al) noexcept { xr_module_delete_aligned(p, static_cast<size_t>(al)); }
void operator delete(void* p, std::size_t, std::align_val_t al) noexcept { xr_module_delete_aligned(p, static_cast<size_t>(al)); }
void operator delete[](void* p, std::size_t, std::align_val_t al) noexcept { xr_module_delete_aligned(p, static_cast<size_t>(al)); }
void operator delete(void* p, std::align_val_t al, const std::nothrow_t&) noexcept { xr_module_delete_aligned(p, static_cast<size_t>(al)); }
void operator delete[](void* p, std::align_val_t al, const std::nothrow_t&) noexcept { xr_module_delete_aligned(p, static_cast<size_t>(al)); }

#endif
