#pragma once
#include <concurrentqueue.h>
#include <span>

#include "MemoryArena.h"
struct testThreadWorkData
{
    int requestdata;
};

//See ThreadPool.h
struct PrototypeThreadWorker
{
    std::span<testThreadWorkData> JobData;

    std::span<uint64_t> OutputFromJobCounters;
    std::span<uint64_t> FinalOutputSumPerThread; //This is really scratch memory, could potentially be allocated by output fn
    
    std::span<size_t>  readbackBufferForQueue;
    moodycamel::ConcurrentQueue<size_t> ResultsReadyAtIndex;
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx);

    void ON_COMPLETE_FN ();
    size_t READ_RESULTS_FN();
};



struct workerContextInterface
{
    static void* contextdata;
    std::span<testThreadWorkData> JobData;
    // static void INITIALIZE_FN(MemoryArena::memoryArena* arena);
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx);
    void ON_COMPLETE_FN ();
    size_t READ_RESULTS_FN(void* threadWorkData);
};



struct PrototypeThreadWorkerReadAtEnd
{
    std::span<uint64_t> contextdata;
    std::span<testThreadWorkData> JobData;
    
    void WORKER_FN( size_t work_item_idx, uint8_t thread_idx);

    size_t READ_IN_PROGRESS_FN();

    void ON_COMPLETE_FN ();
};



