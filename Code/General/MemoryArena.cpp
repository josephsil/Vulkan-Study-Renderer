#include "MemoryArena.h"
#include <windows.h>

MemoryArena::memoryArena::~memoryArena()
{
    VirtualFree(base, 0,MEM_RELEASE);
}

void MemoryArena::initialize(memoryArena* arena, uint32_t size)
{
    arena->base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    arena->head = 0;
    arena->size = size;
}

void* MemoryArena::alloc(memoryArena* a, ptrdiff_t allocSize, ptrdiff_t allocAlignment)
{
    // allocSize = max(allocSize, allocAlignment); //TODO JS: we repeatedly alloc same place if size < alignment
    // assert(allocSize >= allocAlignment);
    void* headPtr = static_cast<char*>(a->base) + a->head;
    size_t sizeLeft = a->size - a->head;
    void* res = std::align(allocAlignment, allocSize, headPtr, sizeLeft);
    assert(res);
    a->last = a->head;
    a->head = (static_cast<char*>(res) + allocSize) - static_cast<char*>(a->base); //bleh
    return res;
}

void MemoryArena::free(memoryArena* a)
{
    a->head = 0;
    a->last = 0;
}

void* MemoryArena::copy(memoryArena* a, void* ptr, size_t size_in_bytes)
{
    void* tgt = alloc(a, size_in_bytes);
    memcpy(tgt, ptr, size_in_bytes);
    return tgt;
}

void MemoryArena::setCursor(memoryArena* a)
{
    a->cursor = a->head;
}

void MemoryArena::freeToCursor(memoryArena* a)
{
    assert(a->cursor >= 0 && a->cursor < a->head);
    a->head = a->cursor;
}


void MemoryArena::freeLast(memoryArena* a)
{
    a->head = a->last;
}

void MemoryArena::RELEASE(memoryArena* a)
{
    VirtualFree(a->base, 0, MEM_RELEASE);
}
