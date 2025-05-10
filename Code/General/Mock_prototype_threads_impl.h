#pragma once
#include <span>

#include "MemoryArena.h"
struct testThreadWorkData
{
    int requestdata;
};

struct workerContext
{
    static void* contextdata;
    static void (*threadWorkerFunction)(void* _thisData, void*,  uint8_t);
    static void (*completeJobFN)(void* _thisData, void*);
};

struct workerContext3
{
    void* contextdata;
    static void threadWorkerFunction(void* _thisData, void* data, uint8_t thread_idx)
    {
        testThreadWorkData* d = (testThreadWorkData*)data; 
        *d = {thread_idx};
    }
    static void completeJobFN (void* _thisData, void* data)
    {
        auto reqspan = std::span((testThreadWorkData*)data, 1200);
        auto dstSpan =  std::span((int*)_thisData, 10); //TODO JS: correct count
        for (auto& result :reqspan)
        {
            printf("%d == \n", result.requestdata);
            dstSpan[result.requestdata] += 1;
        }
        int j = 0;
        size_t ct = 0;
        for (auto result : dstSpan)
        {
            printf("%d: %d  ", j, result);
            j++;
            ct += result;
        }
        printf("\n %llu \n", ct);
    }
};


template<class C> 
class workerContextTemplateExec
{
public:
    void* data;
    void workerFn( void* b, uint8_t _c)
    {
        C::threadWorkerFunction(data, b, _c);
    }
    void completeJobFN(void* b)
    {
        C::completeJobFN(data, b);
    }
};