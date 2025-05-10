#pragma once
#include <span>

#include "MemoryArena.h"


struct testThreadWorkData
{
    int requestdata;
};

struct workerContext2
{
    void* contextdata;
     void threadWorkerFunction(workerContext* _this, void*,  uint8_t);
     void completeJobFN(workerContext* _this, void*);
};


struct workerContext3
{
    void* contextdata;
    //contextdata = MemoryArena::AllocSpan<int>(&differentArena, 10).data(), // 
    void threadWorkerFunction(workerContext* _this, void* data, uint8_t thread_idx)
    {
        testThreadWorkData* d = (testThreadWorkData*)data; 
        *d = {thread_idx};
    };
    void completeJobFN (workerContext* _this, void* data)
    {
        auto reqspan = std::span((testThreadWorkData*)data, 1200);
        auto dstSpan =  std::span((int*)_this->contextdata, 10);
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
    C c;
    static void workerFn( void* b, uint8_t c)
    {
        workerContext* ctx;
        c.threadWorkerFunction(ctx, b, c);
    }
    static void completeJobFN(void* b)
    {
        c.completeJobFN(c.contextdata, b, c);
    }
};

