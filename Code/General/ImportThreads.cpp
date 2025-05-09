#include "ImportThreads.h"
#include <concurrentqueue.h>
#include <vector>
#include <queue>

#include "Array.h"
using namespace std;

const size_t THREAD_CT = 6;
const size_t MAX_REQUESTS = 600;
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
struct ImportThreadsInternals
{
    std::span<std::atomic<TerminationRequest>> CancellationRequest;
    std::span<std::atomic<bool>> debug_isCancelled;
    // moodycamel::ConcurrentQueue<importThreadResult> results;
    Array2<std::atomic<importThreadRequest>> requests;
    Array2<std::atomic<requestStatus>> requestStatuses;
    std::atomic<int> requestCt;
    std::span<std::thread> threadMemory;
};

enum class state {
    POLL,
    WORK,
};

void ThreadFN(ImportThreadsInternals* threadData, uint8_t thread_idx)
{
    state poll = state::POLL;
    int currentRequestindex = thread_idx;
    bool cancelled = false;
    while (!cancelled)
    {
        int requestCt = threadData->requestCt.load(); //Read this atomically before each loop

        if (threadData->CancellationRequest[thread_idx].load() == TerminationRequest::CANCEL) // Accept Cancellation
        {
            cancelled = true; 
            threadData->debug_isCancelled[thread_idx].store(true);
            return;  //Current step is done and has a cancellation request, cancel.
        }
        
        if (!cancelled &&  currentRequestindex != requestCt)
        {
            std::this_thread::sleep_for(std::chrono::nanoseconds(1));
          
            switch (poll)
            {
            case state::POLL:
                {
                    bool newRequest = false;
                
                    while(currentRequestindex< requestCt)
                    {
                        if (threadData->requestStatuses[currentRequestindex].load() == requestStatus::WAITING)
                        {
                            threadData->requestStatuses[currentRequestindex].store(requestStatus::STARTED);
                            poll = state::WORK;
                            break;
                        }
                         currentRequestindex++;
                    }
                    if (!newRequest)
                    {
                        break;
                    }
                break;
                }
    
            case state::WORK:
                {
                    auto workdata = threadData->requests[currentRequestindex].load();
                    printf("%d\n", workdata.requestdata);
                    threadData->requests[currentRequestindex].store(importThreadRequest(-thread_idx)); //todo js -- placeholder to demo work being done
                    threadData->requestStatuses[currentRequestindex].store( requestStatus::COMPLETED);
                    //Finished with work
                    poll = state::POLL;
                    break;
                }
            }
        }
        else
        {
            if (threadData->CancellationRequest[thread_idx].load() == TerminationRequest::COMPLETE)
            {
                return; //Work is done and has a completion request, exit;
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }
    }
    printf("%d: Exited while! \n", thread_idx);
}

void testThreadFn()
{
    printf("%d from thread \n", 1);
}

importThreadRequest::importThreadRequest()
{
    this->requestdata = -1;
}

importThreadRequest::importThreadRequest(int r)
{
    this->requestdata = r;
}

void InitializeImportThreads(MemoryArena::memoryArena* arena, ImportThreads* _init)
{
    _init->_Internal = MemoryArena::Alloc<ImportThreadsInternals>(arena);
    auto& threadPool = *_init->_Internal;
    threadPool.CancellationRequest = MemoryArena::AllocSpan<std::atomic<TerminationRequest>>(arena, THREAD_CT);
    threadPool.debug_isCancelled = MemoryArena::AllocSpan<std::atomic<bool>>(arena, THREAD_CT);

    // threadPool.requests = std::vector<std::atomic<importThreadRequest>>();
    threadPool.requests = MemoryArena::AllocSpan<std::atomic<importThreadRequest>>(arena, MAX_REQUESTS);
    threadPool.requestStatuses = MemoryArena::AllocSpan<std::atomic<requestStatus>>(arena, MAX_REQUESTS);
    threadPool.threadMemory = MemoryArena::AllocSpan<std::thread>(arena, THREAD_CT);
    for (int i = 0; i < THREAD_CT; i++)
    {
        new (&threadPool.CancellationRequest[i]) std::atomic<bool>(false);
        new (&threadPool.debug_isCancelled[i]) std::atomic<bool>(false);
    }
    for(uint8_t thread_id =0; thread_id< THREAD_CT; thread_id++)
    {
           new (&threadPool.threadMemory[thread_id]) std::thread(ThreadFN, _init->_Internal, thread_id);
    }

}


void SubmitRequest(ImportThreads* ThreadPool, int data)
{
    ThreadPool->_Internal->requests.emplace_back((importThreadRequest(data)));
    ThreadPool->_Internal->requestStatuses.emplace_back(requestStatus::WAITING);
    ThreadPool->_Internal->requestCt.store( ThreadPool->_Internal->requestCt.load() + 1);
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
        cancellation_request.store(TerminationRequest::CANCEL);
        
    }
    joinThreads(ThreadPool);
    printf("Cancelled\n");
}

bool WaitForAllRequests(ImportThreads* ThreadPool, int timeout)
{
    auto startTime = clock();
    auto remainingWork = std::vector<size_t>();
    auto indicesToKeep = std::vector<size_t>();
    // int i =0;
    // for (auto& req : ThreadPool->_Internal->requestStatuses.getSpan())
    // {
    //     auto status = req.load();
    //     if (status != requestStatus::COMPLETED)
    //     {
    //         remainingWork.push_back(i);
    //     }
    //     i++;
    // }

    //placeholder, wait for threads to finish
    // while (!remainingWork.empty())
    // {
    //     auto split = ((clock() - startTime) / CLOCKS_PER_SEC) * 1000; //to ms
    //     // if (split > timeout)
    //     // {
    //     //     printf("Aborting thread pool, exceeded timeout %d", timeout);
    //     //     CancelThreadPool(ThreadPool);
    //     //     return false;
    //     // }
    //        std::this_thread::sleep_for(std::chrono::microseconds(20));
    //     
    //     indicesToKeep.clear();
    //     for (auto idx : remainingWork)
    //     {
    //         auto span = ThreadPool->_Internal->requestStatuses.getSpan();
    //         if (span[idx].load()  != requestStatus::COMPLETED)
    //         {
    //             indicesToKeep.push_back(idx);
    //         }
    //     }
    //     remainingWork.resize(indicesToKeep.size());
    //     ranges::copy(indicesToKeep, remainingWork.begin());
    // }

    for(size_t i = 0; i < THREAD_CT; i++)
    {
        printf("sending completion request \n");
        ThreadPool->_Internal->CancellationRequest[i].store(TerminationRequest::COMPLETE);
    }
    printf("Ready to join threads \n");
    //Done, join threads
    joinThreads(ThreadPool);
    printf("joined threads \n");
    printf("Returning \n");
    return true;
    
}