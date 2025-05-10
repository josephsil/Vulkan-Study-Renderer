#pragma once
#include <span>
#include <thread>

const size_t THREAD_CT = 4;
#include "MemoryArena.h"
#include "Mock_prototype_threads_impl.h"

struct ImportThreads;

enum class requestStatus {
    UINITIALIZED,
    WAITING,
    STARTED,
    COMPLETED
};

struct ImportThreadsInternals;


struct ImportThreads
{
    ImportThreadsInternals* _Internal;
};



enum class state {
    POLL,
    WORK,
    CANCELLED
};

state ThreadQueueFnInnerPoll(ImportThreadsInternals* ThreadJobData, size_t currentRequestIndex, uint8_t thread_idx);
void ThreadQueueFnInnerWork(ImportThreadsInternals* ThreadJobData, size_t currentRequestIndex, int8_t thread_idx);

//move?
bool TryReserveRequestData(ImportThreadsInternals* d, size_t thisThreadIdx, size_t tgtIndex);
void ReleaseRequestData(ImportThreadsInternals* d, size_t thisThreadIdx, size_t tgtIndex);
size_t GetRequestCt(ImportThreadsInternals* ThreadJobData);
void* UnwrapWorkerDataForIdx(ImportThreadsInternals* ThreadJobData, size_t requestIndex);
void* GetWorkerDataPtr(ImportThreadsInternals* ThreadJobData);
bool ShouldComplete(ImportThreadsInternals* ThreadJobData, uint8_t thread_idx);

//get rid of this
size_t Get_IncrementRequestCt(ImportThreadsInternals* ThreadJobData);
//TODO JS: in the process of splitting this up, so I can pull this functio out to the header and make templte rather than void pointers
//TODO JS: Need to hide all of the importthreadinternals details
//TODO JS currently workercontext is all pointers so I can pass by value, but this might need attention with templaet path


template<class T>
inline void ThreadQueueFn(ImportThreadsInternals* ThreadJobData, workerContextTemplateExec<T> context, uint8_t thread_idx)
{
    state poll = state::POLL;
    size_t currentRequestindex = thread_idx;
    bool cancelled = false;
    // auto& requestsData = ThreadJobData->requestsData;

    size_t requestCt = GetRequestCt(ThreadJobData); //Read this atomically before each loop
      

        while(true)
        {
         if(currentRequestindex < requestCt)
         {
             switch (poll)
             {
             case  state::POLL:
                 {
                     currentRequestindex = Get_IncrementRequestCt(ThreadJobData); //Get the next request to try to read
                     if(currentRequestindex >= requestCt)
                     {
                         break;
                     }
                     poll = ThreadQueueFnInnerPoll(ThreadJobData, currentRequestindex, thread_idx);
                     break;
                 }
                 
             case state::WORK:
                 {
                     if (!TryReserveRequestData(ThreadJobData, thread_idx, currentRequestindex))
                     {
                         assert(!"Unexpected reserved request data");
                     }

                     ThreadQueueFnInnerWork(ThreadJobData, currentRequestindex, thread_idx);
                     void* dataPtr = (UnwrapWorkerDataForIdx(ThreadJobData, currentRequestindex));
                     context.workerFn(dataPtr,  thread_idx);
                       
                     ReleaseRequestData(ThreadJobData, thread_idx, currentRequestindex);
                     //Finished with work
                     poll = state::POLL;
                     break;
                 }
             case state::CANCELLED:
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
                // std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
        }
    printf("%d: Exited while! \n", thread_idx);
}

std::thread* GetThreadAllocation(ImportThreads* threadPool, size_t idx);

template<class T>
void CreateThreads(ImportThreads* threadPool, workerContextTemplateExec<T> context)
{
    for(uint8_t thread_id =0; thread_id< THREAD_CT; thread_id++)
    {
        printf("%d thead id \n", thread_id);
     
        new (GetThreadAllocation(threadPool, thread_id)) std::thread(ThreadQueueFn<T>, threadPool->_Internal, context, thread_id);
    }
}
bool WaitForCompletionInternal(ImportThreads* ThreadPool);

template<class T>
bool WaitForCompletion(ImportThreads* ThreadPool, workerContextTemplateExec<T> context, int timeout  = 12400)
{

    auto res = WaitForCompletionInternal(ThreadPool);
    assert(res);
    
    context.completeJobFN(GetWorkerDataPtr(ThreadPool->_Internal));

    return true;
    
}

    void InitializeThreadPool(MemoryArena::memoryArena* arena, ImportThreads* _init, void* workData, ptrdiff_t requestSize, size_t maxRequests);
    void SubmitRequests(ImportThreads* threadPool, size_t data);


    void CreateThreads(ImportThreads* threadPool, workerContext t);

