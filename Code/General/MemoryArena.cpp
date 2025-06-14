#include "MemoryArena.h"
#include <windows.h>

MemoryArena::Allocator::~Allocator()
{
	assert(base != nullptr);
    VirtualFree(base, 0,MEM_RELEASE);
}

void MemoryArena::Initialize(Allocator* arena, uint32_t size)
{
    arena->base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    arena->head = 0;
    arena->size = size;
}

void* MemoryArena::allocbytes(Allocator* a, ptrdiff_t allocSize, ptrdiff_t allocAlignment)
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

void ZeroInitializeRegion(ArenaAllocator a, ptrdiff_t start, ptrdiff_t size)
{
	memset((void*)((char*)a->base + start), 0, size);
}

void MemoryArena::FreeZeroInitialize(Allocator* a)
{
	ZeroInitializeRegion(a, 0, a->last);
    a->head = 0;
    a->last = 0;
}

void MemoryArena::Free(Allocator* a)
{
    //Should probably just always do this
#if _DEBUG
    FreeZeroInitialize(a);
    return;
#endif
    a->head = 0;
    a->last = 0;
}

void* MemoryArena::alloccopybytes(Allocator* a, void* ptr, size_t size_in_bytes)
{
    void* tgt = allocbytes(a, size_in_bytes);
    memcpy(tgt, ptr, size_in_bytes);
    return tgt;
}

ptrdiff_t MemoryArena::GetCurrentOffset(Allocator* a)
{
    auto out = a->head;
    a->cursor = a->head;
    return out;
}

void MemoryArena::FreeToOffset(Allocator* a, ptrdiff_t cursor)
{
	#if _DEBUG
	ZeroInitializeRegion(a, cursor, a->last - cursor);
	#endif 
    a->head = cursor;
}

