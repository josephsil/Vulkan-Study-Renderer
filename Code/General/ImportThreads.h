#pragma once
#include <span>

#include "MemoryArena.h"

enum class requestStatus {
    UINITIALIZED,
    WAITING,
    STARTED,
    COMPLETED
};
struct importThreadRequest
{
    int requestdata;
    importThreadRequest();
    importThreadRequest(int r);
};

struct ImportThreadsInternals;
struct ImportThreads
{
    ImportThreadsInternals* _Internal;
};
    void InitializeImportThreads(MemoryArena::memoryArena* arena, ImportThreads* _init);
    void SubmitRequest(ImportThreads* ThreadPool, int data);
    bool WaitForAllRequests(ImportThreads* ThreadPool, int timeout  = 12400);


