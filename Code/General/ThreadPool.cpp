#include "ThreadPool.h"
#include <concurrentqueue.h>
#include <vector>
#include <queue>

#include "Array.h"
#include "PrototypeThreadWorker.h"
using namespace std;

struct available //cpp thread?
{
    bool completed;
};

enum class ThreadTerminationRequest
{
    NONE,
    CANCEL,
    COMPLETE
};

struct WorkItemData
{
    //Length of THREAD_CT
    std::span<std::atomic<size_t>> perThreadLockedIndices;
    //What range (index, for now) of the data arrays are locked per thread
    std::atomic<size_t> requestCt;
    std::atomic<size_t> requestsIndex;

    void* requestDataPayload;
    
    /**
     * 
     */
    Array<ThreadPool::WorkItemState> WorkItemRequests;
};

struct ThreadPool_INTERNAL::ThreadPoolInternals
{
    std::span<std::atomic<uint32_t>> CancellationRequest;
    //Length of THREAD_CT //todo js: order of this matters -- exception if its first
    std::span<std::atomic<bool>> debug_isCancelled; //Length of THREAD_CT
    //Result data:
    WorkItemData workItemsData;
    std::span<std::thread> threadMemory;
};


bool ThreadPool::TryReserveRequestData(ThreadPool_INTERNAL::ThreadPoolInternals* d, size_t thisThreadIdx, size_t tgtIndex)
{
    //Check if any thread already has this reserved
    auto& lockedIndices = d->workItemsData.perThreadLockedIndices;
    if (!ThreadPool::CheckRequestDataReadable(*d, thisThreadIdx, tgtIndex))
    {
        return false;
    }

    //Reserve 
    lockedIndices[thisThreadIdx].store(tgtIndex);
    return true;
}

void ThreadPool::ReleaseRequestData(ThreadPool_INTERNAL::ThreadPoolInternals* d, size_t thisThreadIdx, size_t tgtIndex)
{
    //Reserve 
    d->workItemsData.perThreadLockedIndices[thisThreadIdx].store(SIZE_MAX);
}


ThreadPool::WorkerState ThreadPool::ThreadQueueFnInnerPoll(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData, size_t currentRequestIndex, uint8_t thread_idx)
{
    auto& requestsData = ThreadJobData->workItemsData;
    size_t requestCt = requestsData.requestCt; //Read this atomically before each loop

    if (ThreadJobData->CancellationRequest[thread_idx] == static_cast<uint32_t>(ThreadTerminationRequest::CANCEL))
    // Accept Cancellation
    {
        // cancelled = true; 
        ThreadJobData->debug_isCancelled[thread_idx].store(true);
        return ThreadPool::WorkerState::CANCELLED; //Current step is done and has a cancellation request, cancel.
    }

    if (!ThreadPool::TryReserveRequestData(ThreadJobData, thread_idx, currentRequestIndex))
    {
        assert(!"Unexpected reserved request data");
    }
    assert(requestsData.WorkItemRequests[currentRequestIndex] == WorkItemState::WAITING);

    requestsData.WorkItemRequests[currentRequestIndex] = ThreadPool::WorkItemState::STARTED;
    ThreadPool::ReleaseRequestData(ThreadJobData, thread_idx, currentRequestIndex);
    return ThreadPool::WorkerState::WORK;
}

size_t ThreadPool::GetRequestCt(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData)
{
    return ThreadJobData->workItemsData.requestCt;
}

bool ThreadPool::ShouldComplete(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData, uint8_t thread_idx)
{
    return ThreadJobData->CancellationRequest[thread_idx].load() == static_cast<uint32_t>(
        ThreadTerminationRequest::COMPLETE);
}

size_t ThreadPool::Get_IncrementRequestCt(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData)
{
    return ThreadJobData->workItemsData.requestsIndex++; //Get the next request to try to read
}

size_t ThreadPool::Get_RequestCt(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData)
{
    return ThreadJobData->workItemsData.requestsIndex; //Get the next request to try to read
}

std::thread* ThreadPool::GetThreadAllocation(ThreadPool::Pool* threadPool, size_t idx)
{
    return &threadPool->_Internal->threadMemory[idx];
}

void joinThreads(ThreadPool::Pool* ThreadPool)
{
    for (auto& t : ThreadPool->_Internal->threadMemory)
    {
        t.join();
    }
}

bool ThreadPool::WaitForCompletionInternal(ThreadPool::Pool* ThreadPool)
{
    for (size_t i = 0; i < ThreadPool->THREAD_CT; i++)
    {
        ThreadPool->_Internal->CancellationRequest[i].store(static_cast<uint32_t>(ThreadTerminationRequest::COMPLETE));
    }
    //Done, join threads
    joinThreads(ThreadPool);
    printf("joined threads \n");
    return true;
}

void ThreadPool::MarkWorkItemCompleted(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData, size_t currentRequestIndex, int8_t thread_idx)
{
    ThreadJobData->workItemsData.WorkItemRequests[currentRequestIndex] = ThreadPool::WorkItemState::COMPLETED;
    //do actual work
}

bool ThreadPool::CheckRequestDataReadable(ThreadPool_INTERNAL::ThreadPoolInternals& d, size_t thisThreadIdx, size_t tgtIndex)
{
    auto lockedIndices = d.workItemsData.perThreadLockedIndices.data();
    for (size_t i = 0; i < d.threadMemory.size(); i++)
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

void ThreadPool::InitializeThreadPool(MemoryArena::memoryArena* arena, ThreadPool::Pool* _init, size_t maxRequests,
                          size_t THREAD_CT)
{
    _init->_Internal = MemoryArena::AllocDefaultInitialize<ThreadPool_INTERNAL::ThreadPoolInternals>(arena);
    _init->THREAD_CT = THREAD_CT;
    auto& threadPool = *_init->_Internal;
    threadPool.debug_isCancelled = MemoryArena::AllocSpan<std::atomic<bool>>(arena, THREAD_CT);
    threadPool.CancellationRequest = MemoryArena::AllocSpan<std::atomic<uint32_t>>(arena, THREAD_CT);
    for (int i = 0; i < THREAD_CT; i++)
    {
        new(&threadPool.debug_isCancelled[i]) std::atomic<bool>(false);
        new(&threadPool.CancellationRequest[i]) std::atomic<uint32_t>(
            static_cast<uint32_t>(ThreadTerminationRequest::NONE));
    }

    // threadPool.requests = std::vector<std::atomic<importThreadRequest>>();
    threadPool.workItemsData.perThreadLockedIndices = MemoryArena::AllocSpanEmplaceInitialize<std::atomic<size_t>>(
        arena, THREAD_CT, SIZE_MAX);
    threadPool.threadMemory = MemoryArena::AllocSpan<std::thread>(arena, THREAD_CT);
    threadPool.workItemsData.WorkItemRequests = MemoryArena::AllocSpan<ThreadPool::WorkItemState>(arena, maxRequests);
}


void ThreadPool::SubmitRequests(ThreadPool::Pool* threadPool, size_t ct)
{
    //We can safely push back to the list without sync objects, because threads don't try to read beyond requestct
    for (size_t i = 0; i < ct; i++)
    {
        threadPool->_Internal->workItemsData.WorkItemRequests.emplace_back(ThreadPool::WorkItemState::WAITING);
    }
    threadPool->_Internal->workItemsData.requestCt += ct;
}


void CancelThreadPool(ThreadPool::Pool* ThreadPool)
{
    printf("Cancelling\n");
    bool allCancelld = false;
    for (auto& cancellation_request : ThreadPool->_Internal->CancellationRequest)
    {
        cancellation_request.store(static_cast<uint32_t>(ThreadTerminationRequest::CANCEL));
    }
    joinThreads(ThreadPool);
    printf("Cancelled\n");
}
