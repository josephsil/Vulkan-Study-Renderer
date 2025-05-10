#pragma once
#include <span>

#include "MemoryArena.h"

enum class requestStatus {
    UINITIALIZED,
    WAITING,
    STARTED,
    COMPLETED
};

struct ImportThreadsInternals;

struct workerContext
{
    void* contextdata;
     void (*threadWorkerFunction)(workerContext* _this, void*,  uint8_t);
     void (*completeJobFN)(workerContext* _this, void*);
};



struct ImportThreads
{
    ImportThreadsInternals* _Internal;
};
    void InitializeThreadPool(MemoryArena::memoryArena* arena, ImportThreads* _init, void* workData, ptrdiff_t requestSize, size_t maxRequests);
    void SubmitRequests(ImportThreads* threadPool, size_t data);
    bool WaitForCompletion(ImportThreads* ThreadPool, workerContext workerContext, int timeout  = 12400);

    void CreateThreads(ImportThreads* threadPool, workerContext t);