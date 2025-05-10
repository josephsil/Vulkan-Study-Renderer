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
void* UnwrapWorkerData(ImportThreadsInternals* ThreadJobData, size_t requestIndex);
bool ShouldComplete(ImportThreadsInternals* ThreadJobData, uint8_t thread_idx);

//get rid of this
size_t Get_IncrementRequestCt(ImportThreadsInternals* ThreadJobData);
//TODO JS: in the process of splitting this up, so I can pull this functio out to the header and make templte rather than void pointers
//TODO JS: Need to hide all of the importthreadinternals details
//TODO JS currently workercontext is all pointers so I can pass by value, but this might need attention with templaet path
inline void ThreadQueueFn(ImportThreadsInternals* ThreadJobData, workerContext workerContext, uint8_t thread_idx)
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
                     void* dataPtr = (UnwrapWorkerData(ThreadJobData, currentRequestindex));
                     workerContext.threadWorkerFunction(&workerContext, dataPtr,  thread_idx);
                       
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

struct ImportThreads
{
    ImportThreadsInternals* _Internal;
};
    void InitializeThreadPool(MemoryArena::memoryArena* arena, ImportThreads* _init, void* workData, ptrdiff_t requestSize, size_t maxRequests);
    void SubmitRequests(ImportThreads* threadPool, size_t data);
    bool WaitForCompletion(ImportThreads* ThreadPool, workerContext workerContext, int timeout  = 12400);

    void CreateThreads(ImportThreads* threadPool, workerContext t);

