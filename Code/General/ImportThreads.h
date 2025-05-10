#pragma once
#include <span>
#include <thread>

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
    size_t THREAD_CT;
    ImportThreadsInternals* _Internal;
};



enum class state {
    POLL,
    WORK,
    CANCELLED
};

template<class C>
//Not strictly sure the wrapper is necessary, I might be able to pass references to objects of type C directly to the thread functions
//However, didn't work on first pass. Not sure I understand template intiialization.
class ThreadRunnerWrapper //todo js: need a better name for this. basically just screwing around with templates and decltype and stuff at this point
{
public:
    C* input;
    //Wanted to do stuffl ike this, but oculdnt get it to work
    // template <typename ClassType, typename MethodType>
    // using FunctionSignature = std::remove_pointer_t<std::remove_cv_t<MethodType>>;
        
    // static_assert( std::is_same< FunctionSignature<C, decltype(&C::WORKER_FN)>,FunctionSignature<workerContextInterface,decltype(&workerContextInterface::WORKER_FN)>>::value,
    //   "has threadWorkerFunction");

    ThreadRunnerWrapper(C* c)
    {
        input = c;
    }
    void workerFn( void* b, uint8_t _c)
    {
        input->WORKER_FN(b, _c);
    }
    void completeJobFN(void* b)
    {
        input->ON_COMPLETE_FN(b);
    }
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
inline void ThreadQueueFn(ImportThreadsInternals* ThreadJobData, ThreadRunnerWrapper<T> context, uint8_t thread_idx)
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
void CreateThreads(ImportThreads* threadPool, ThreadRunnerWrapper<T> context)
{
    for(uint8_t thread_id =0; thread_id< threadPool->THREAD_CT; thread_id++)
    {
        printf("%d thead id \n", thread_id);
     
        new (GetThreadAllocation(threadPool, thread_id)) std::thread(ThreadQueueFn<T>, threadPool->_Internal, context, thread_id);
    }
}
bool WaitForCompletionInternal(ImportThreads* ThreadPool);

template<class T>
bool WaitForCompletion(ImportThreads* ThreadPool, ThreadRunnerWrapper<T> context, int timeout  = 12400)
{

    auto res = WaitForCompletionInternal(ThreadPool);
    assert(res);
    
    context.completeJobFN(GetWorkerDataPtr(ThreadPool->_Internal));

    return true;
    
}

    void InitializeThreadPool(MemoryArena::memoryArena* arena, ImportThreads* _init, void* workData, ptrdiff_t requestSize, size_t maxRequests, size_t THREAD_CT);
    void SubmitRequests(ImportThreads* threadPool, size_t data);


    void CreateThreads(ImportThreads* threadPool, workerContextInterface t);

