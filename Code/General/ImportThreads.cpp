#include "ImportThreads.h"
#include <concurrentqueue.h>
#include <vector>
#include <queue>

#include "Array.h"
#include "Mock_prototype_threads_impl.h"
using namespace std;

const size_t THREAD_CT = 4;
struct available //cpp thread?
{
    bool completed;
    
};

enum class TerminationRequest
{
    NONE,
    CANCEL,
    COMPLETE
};


struct importThreadResult
{
    bool done;
};
struct RequestsData
{
    //Length of THREAD_CT
    std::span<std::atomic<size_t>> perThreadLockedIndices; //What range (index, for now) of the data arrays are locked per thread
    std::atomic<size_t> requestCt;
    std::atomic<size_t> requestsIndex;
    
    void* requestDataPayload;
    ptrdiff_t requestDataTypeSize;
    Array<requestStatus> requestStatuses;
    
};
struct ImportThreadsInternals
{
    std::span<std::atomic<uint32_t>> CancellationRequest; //Length of THREAD_CT //todo js: order of this matters -- exception if its first
    std::span<std::atomic<bool>> debug_isCancelled; //Length of THREAD_CT
    //Result data:
    RequestsData requestsData;
    std::span<std::thread> threadMemory;
};
bool CheckRequestDataReadable(ImportThreadsInternals& d, size_t thisThreadIdx, size_t tgtIndex)
{
    auto lockedIndices = d.requestsData.perThreadLockedIndices.data();
    for (size_t i = 0; i < THREAD_CT; i++)
    {
        if (i == thisThreadIdx) continue;
        size_t comp = lockedIndices[i].load();
        if (comp == tgtIndex)
        {
            return false;
        }
    }
    return true;
}

bool TryReserveRequestData(ImportThreadsInternals* d, size_t thisThreadIdx, size_t tgtIndex)
{

    //Check if any thread already has this reserved
    auto& lockedIndices = d->requestsData.perThreadLockedIndices;
    if (!CheckRequestDataReadable(*d,thisThreadIdx, tgtIndex))
    {
        return false;
    }

    //Reserve 
    lockedIndices[thisThreadIdx].store(tgtIndex);
    return true;
}

void ReleaseRequestData(ImportThreadsInternals* d, size_t thisThreadIdx, size_t tgtIndex)
{
    //Reserve 
    d->requestsData.perThreadLockedIndices[thisThreadIdx].store(SIZE_MAX);
}



state ThreadQueueFnInnerPoll(ImportThreadsInternals* ThreadJobData, size_t currentRequestIndex, uint8_t thread_idx)
{
    auto& requestsData = ThreadJobData->requestsData;
    size_t requestCt = requestsData.requestCt; //Read this atomically before each loop

    if (ThreadJobData->CancellationRequest[thread_idx]== (uint32_t)TerminationRequest::CANCEL) // Accept Cancellation
    {
        // cancelled = true; 
        ThreadJobData->debug_isCancelled[thread_idx].store(true);
        return state::CANCELLED;  //Current step is done and has a cancellation request, cancel.
    }

    if (!TryReserveRequestData(ThreadJobData, thread_idx, currentRequestIndex))
    {
        assert(!"Unexpected reserved request data");
    }
    assert(requestsData.requestStatuses[currentRequestIndex] == requestStatus::WAITING);
                
    requestsData.requestStatuses[currentRequestIndex] = requestStatus::STARTED;
    ReleaseRequestData(ThreadJobData, thread_idx, currentRequestIndex);
    return state::WORK;
       
}

size_t GetRequestCt(ImportThreadsInternals* ThreadJobData)
{
    return ThreadJobData->requestsData.requestCt;
}

void* UnwrapWorkerData(ImportThreadsInternals* ThreadJobData, size_t requestIndex)
{
    return (char*)ThreadJobData->requestsData.requestDataPayload + (ThreadJobData->requestsData.requestDataTypeSize * requestIndex);
}

bool ShouldComplete(ImportThreadsInternals* ThreadJobData, uint8_t thread_idx)
{
    return ThreadJobData->CancellationRequest[thread_idx].load() == (uint32_t)TerminationRequest::COMPLETE;
}

size_t Get_IncrementRequestCt(ImportThreadsInternals* ThreadJobData)
{
    return ThreadJobData->requestsData.requestsIndex++; //Get the next request to try to read
}

void ThreadQueueFnInnerWork(ImportThreadsInternals* ThreadJobData, size_t currentRequestIndex, int8_t thread_idx)
{
 
    ThreadJobData->requestsData.requestStatuses[currentRequestIndex] = requestStatus::COMPLETED;
    //do actual work
    
}



void testThreadFn()
{
    printf("%d from thread \n", 1);
}

void InitializeThreadPool(MemoryArena::memoryArena* arena, ImportThreads* _init, void* workData, ptrdiff_t requestSize, size_t maxRequests)
{
    _init->_Internal = MemoryArena::AllocDefaultInitialize<ImportThreadsInternals>(arena);
    auto& threadPool = *_init->_Internal;
    threadPool.debug_isCancelled = MemoryArena::AllocSpan<std::atomic<bool>>(arena, THREAD_CT);
    threadPool.CancellationRequest = MemoryArena::AllocSpan<std::atomic<uint32_t>>(arena, THREAD_CT);
    for(int i =0; i < THREAD_CT; i++)
    {
        new (&threadPool.debug_isCancelled[i]) std::atomic<bool>(false);
        new (&threadPool.CancellationRequest[i]) std::atomic<uint32_t>((uint32_t)TerminationRequest::NONE);
    }

    // threadPool.requests = std::vector<std::atomic<importThreadRequest>>();
    threadPool.requestsData.perThreadLockedIndices = MemoryArena::AllocSpanEmplaceInitialize<std::atomic<size_t>>(arena, THREAD_CT, SIZE_MAX);
    threadPool.requestsData.requestDataPayload = workData;
    threadPool.requestsData.requestDataTypeSize = requestSize;
    threadPool.threadMemory = MemoryArena::AllocSpan<std::thread>(arena, THREAD_CT);
    threadPool.requestsData.requestStatuses = MemoryArena::AllocSpan<requestStatus>(arena, maxRequests);

}

void CreateThreads(ImportThreads* threadPool, workerContext functions)
{
    for(uint8_t thread_id =0; thread_id< THREAD_CT; thread_id++)
    {
        printf("%d thead id \n", thread_id);
     
        new (&threadPool->_Internal->threadMemory[thread_id]) std::thread(ThreadQueueFn, threadPool->_Internal, functions, thread_id);
    }
}


void SubmitRequests(ImportThreads* threadPool, size_t ct)
{
    //We can safely push back to the list without sync objects, because threads don't try to read beyond requestct
    for(size_t i =0; i < ct; i++)
    {
        threadPool->_Internal->requestsData.requestStatuses.emplace_back(requestStatus::WAITING);
    }
        threadPool->_Internal->requestsData.requestCt += ct;
}

void joinThreads(ImportThreads* ThreadPool)
{
    for (auto& t : ThreadPool->_Internal->threadMemory)
    {
        t.join();
    }
}


void CancelThreadPool(ImportThreads* ThreadPool)
{
    printf("Cancelling\n");
    bool allCancelld = false;
    for (auto& cancellation_request : ThreadPool->_Internal->CancellationRequest)
    {
        cancellation_request.store((uint32_t)TerminationRequest::CANCEL);
        
    }
    joinThreads(ThreadPool);
    printf("Cancelled\n");
}

bool WaitForCompletion(ImportThreads* ThreadPool, workerContext workerContext, int timeout)
{
    auto startTime = clock();
    auto remainingWork = std::vector<size_t>();
    auto indicesToKeep = std::vector<size_t>();
    for(size_t i = 0; i < THREAD_CT; i++)
    {
        printf("sending completion request \n");
        ThreadPool->_Internal->CancellationRequest[i].store(((uint32_t)TerminationRequest::COMPLETE));
    }
    //Done, join threads
    joinThreads(ThreadPool);
    printf("joined threads \n");
 

    workerContext.completeJobFN(&workerContext, ThreadPool->_Internal->requestsData.requestDataPayload);

    return true;
    
}