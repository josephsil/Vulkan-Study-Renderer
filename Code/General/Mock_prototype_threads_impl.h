#pragma once
#include <span>

#include "MemoryArena.h"
struct testThreadWorkData
{
    int requestdata;
};







struct PrototypeThreadWorker
{
    std::span<uint64_t> contextdata;
    void WORKER_FN(void* data, uint8_t thread_idx)
    {
        testThreadWorkData* d = (testThreadWorkData*)data; 
        *d = {thread_idx};
    }
    void ON_COMPLETE_FN (void* data)
    {
        auto reqspan = std::span((testThreadWorkData*)data, 1200);
        auto dstSpan =  contextdata; //TODO JS: correct count
        for (auto& result :reqspan)
        {
            printf("%d == \n", result.requestdata);
            dstSpan[result.requestdata] += 1;
        }
        int j = 0;
        size_t ct = 0;
        for (auto result : dstSpan)
        {
            printf("%d: %llu  ", j, result);
            j++;
            ct += result;
        }
        printf("\n %llu \n", ct);
    }
};

struct workerContextInterface
{
    static void* contextdata;
    // static void INITIALIZE_FN(MemoryArena::memoryArena* arena);
    void WORKER_FN(void* data, uint8_t thread_idx);
    void ON_COMPLETE_FN (void* data);
};


