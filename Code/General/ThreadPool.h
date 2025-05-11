#pragma once
#include <span>
#include <thread>

#include "MemoryArena.h"

namespace ThreadPool_INTERNAL {
    struct ThreadPoolInternals;
}
namespace ThreadPool {
    /**
    * // USAGE EXAMPLE:
    *\n      MemoryArena::memoryArena threadPoolAllocator  {};
    *\n    initialize(&threadPoolAllocator, 3 * 1000000);
    *\n
    *\n    //Initialize a pool
    *\n         ThreadPool::Pool threadPool;
    *\n         int workItemCt = 1200;
    *\n         int threadCt =  4;
    *\n         InitializeThreadPool(&threadPoolAllocator, &threadPool, workItemCt, threadCt);
    *\n
    *\n    //Example of how to set up a thread worker. See prototypethreadworker.h for implementation  
    *\n          size_t ReadbackBufferSize = 50;
    *\n          PrototypeThreadWorker ThreadWorker = 
    *\n          {
    *\n              .JobData =  MemoryArena::AllocSpan<testThreadWorkData>(&threadPoolAllocator, workItemCt),
    *\n              .OutputFromJobCounters =  MemoryArena::AllocSpan<uint64_t>(&threadPoolAllocator, workItemCt),
    *\n              .FinalOutputSumPerThread = MemoryArena::AllocSpan<uint64_t>(&threadPoolAllocator, threadCt),
    *\n              .readbackBufferForQueue =   MemoryArena::AllocSpan<size_t>(&threadPoolAllocator, ReadbackBufferSize),
    *\n              .ResultsReadyAtIndex = moodycamel::ConcurrentQueue<size_t>()
    *\n          };
    *\n
    *\n    //Create a jobwrapper to pass to the thread pool, create threads 
    *\n          auto JobDataWrapper = ThreadPool::ThreadWorkWrapper(&ThreadWorker);
    *\n          CreateThreads(&threadPool,JobDataWrapper );
    *\n
    *\n    //Tell the thread pool workdata.size() requests are ready for it
    *\n          SubmitRequests(&threadPool, workData.size());
    *\n    //Optional -- readbackdata gets called in a loop until it returns 0 (to indicate an empty queue) on waitforcompletion
    *\n          size_t resultsRead = 0;
    *\n          while(resultsRead < workItemCt)
    *\n          {
    *\n              resultsRead += ReadBackData(&threadPool, JobDataWrapper);
    *\n          }
    *\n    //Join the threads, call readback data and read results
    *\n         WaitForCompletion(&threadPool, JobDataWrapper);
     */
    struct Pool;

    enum class WorkItemState
    {
        UINITIALIZED,
        WAITING,
        STARTED,
        COMPLETED
    };

    enum class WorkerState
    {
        POLL,
        WORK,
        CANCELLED
    };




    struct Pool
    {
        size_t THREAD_CT;
        ThreadPool_INTERNAL::ThreadPoolInternals* _Internal;
    };


    template <class C>
    //Not strictly sure the wrapper is necessary, I might be able to pass references to objects of type C directly to the thread functions
    //However, didn't work on first pass. Not sure I understand template intiialization.
    struct ThreadWorkWrapper
        //todo js: need a better name for this. basically just screwing around with templates and decltype and stuff at this point
    {

        C* input;
        //Wanted to do stufflike this, but oculdnt get it to work:
        // template <typename ClassType, typename MethodType>
        // using FunctionSignature = std::remove_pointer_t<std::remove_cv_t<MethodType>>;

        // static_assert( std::is_same< FunctionSignature<C, decltype(&C::WORKER_FN)>,FunctionSignature<workerContextInterface,decltype(&workerContextInterface::WORKER_FN)>>::value,
        //   "has threadWorkerFunction");

        ThreadWorkWrapper(C* c)
        {
            input = c;
        }

        void ExecuteWork(size_t work_item_idx, uint8_t _c)
        {
            input->WORKER_FN(work_item_idx, _c);
        }

        size_t ReadBackResults()
        {
            return input->READ_RESULTS_FN();
        }

        void CompletePool()
        {
            //Read all remaining results 
            while(true)
            {
                //Break as soon as we get 0 results back, which indicates an empty queue 
                if (!input->READ_RESULTS_FN())  break;
            }
            input->ON_COMPLETE_FN();
        }
    };

    WorkerState ThreadQueueFnInnerPoll(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData, size_t currentRequestIndex, uint8_t thread_idx);
    void MarkWorkItemCompleted(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData, size_t currentRequestIndex, int8_t thread_idx);

    bool CheckRequestDataReadable(ThreadPool_INTERNAL::ThreadPoolInternals& d, size_t thisThreadIdx, size_t tgtIndex);
    bool TryReserveRequestData(ThreadPool_INTERNAL::ThreadPoolInternals* d, size_t thisThreadIdx, size_t tgtIndex);
    void ReleaseRequestData(ThreadPool_INTERNAL::ThreadPoolInternals* d, size_t thisThreadIdx, size_t tgtIndex);
    size_t GetRequestCt(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData);
    bool ShouldComplete(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData, uint8_t thread_idx);

    //get rid of this
    size_t Get_IncrementRequestCt(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData);

    template <class T>
    void ThreadQueueFn(ThreadPool_INTERNAL::ThreadPoolInternals* ThreadJobData, ThreadWorkWrapper<T> context, uint8_t thread_idx)
    {
        auto poll = WorkerState::POLL;
        size_t currentRequestindex = thread_idx;
        bool cancelled = false;
        // auto& workItemsData = ThreadJobData->workItemsData;

        size_t requestCt = GetRequestCt(ThreadJobData); //Read this atomically before each loop


        while (true)
        {
            if (currentRequestindex < requestCt)
            {
                switch (poll)
                {
                case WorkerState::POLL:
                {
                    currentRequestindex = Get_IncrementRequestCt(ThreadJobData); //Get the next request to try to read
                    if (currentRequestindex >= requestCt)
                    {
                        break;
                    }
                    poll = ThreadQueueFnInnerPoll(ThreadJobData, currentRequestindex, thread_idx);
                    break;
                }

                case WorkerState::WORK:
                {
                    if (!TryReserveRequestData(ThreadJobData, thread_idx, currentRequestindex))
                    {
                        assert(!"Unexpected reserved request data");
                    }
                    MarkWorkItemCompleted(ThreadJobData, currentRequestindex, thread_idx);
                    context.ExecuteWork(currentRequestindex, thread_idx);
                    ReleaseRequestData(ThreadJobData, thread_idx, currentRequestindex);
                    //Finished with work
                    poll = WorkerState::POLL;
                    break;
                }
                case WorkerState::CANCELLED:
                {
                    cancelled = true;
                    break;
                }
                }
            }


            else
            {
                if (ShouldComplete(ThreadJobData, thread_idx))
                {
                    return; //Work is done and has a completion request, exit;
                }
            }
        }
    }

    std::thread* GetThreadAllocation(Pool* threadPool, size_t idx);

    template <class T>
    void CreateThreads(Pool* threadPool, ThreadWorkWrapper<T> context)
    {
        for (uint8_t thread_id = 0; thread_id < threadPool->THREAD_CT; thread_id++)
        {
            printf("%d thead id \n", thread_id);

            new(GetThreadAllocation(threadPool, thread_id)) std::thread(ThreadQueueFn<T>, threadPool->_Internal, context, thread_id);
        }
    }

    template <class T>
    size_t ReadBackData(Pool* ThreadPool, ThreadWorkWrapper<T> context)
    {
        return context.ReadBackResults();
    }

    bool WaitForCompletionInternal(Pool* ThreadPool);

    template <class T>
    bool WaitForCompletion(Pool* ThreadPool, ThreadWorkWrapper<T> context, int timeout = 12400)
    {
        auto res = WaitForCompletionInternal(ThreadPool);
        assert(res);

        
        context.CompletePool();

        return true;
    }

    void InitializeThreadPool(MemoryArena::memoryArena* arena, Pool* _init, size_t maxRequests,
        size_t THREAD_CT);
    void SubmitRequests(Pool* threadPool, size_t data);

}