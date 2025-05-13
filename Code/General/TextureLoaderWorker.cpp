#include "TextureLoaderWorker.h"

#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/CommandPoolManager.h"

void TextureLoaderThreadWorker::WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
{

    //TODO JS: Need per-thread context. Should pull the context ouside of TextureCreationInfoArgs to make this less grody
    ThreadsOutput[work_item_idx] = TextureCreation::CreateTextureFromArgs_Start( PerThreadContext[thread_idx], ThreadsInput[work_item_idx]);
    ResultsReadyAtIndex.enqueue(work_item_idx);
}

void TextureLoaderThreadWorker::ON_COMPLETE_FN()
{
    for (auto ctx : PerThreadContext)
    {
        ctx.threadDeletionQueue->FreeQueue();
    }
    printf("Completed\n");
}

unsigned long long TextureLoaderThreadWorker::READ_RESULTS_FN()
{
    size_t dequeueCt = ResultsReadyAtIndex.try_dequeue_bulk(readbackBufferForQueue.begin(),
                                                         readbackBufferForQueue.size());
    for (int i = 0; i < dequeueCt; i++)
    {
        size_t indexReadyToRead = readbackBufferForQueue[i];
        auto& readData = ThreadsOutput[indexReadyToRead];
        FinalOutput[indexReadyToRead] =  TextureCreation::CreateTextureFromArgsFinalize(MainThreadContext, ThreadsOutput[indexReadyToRead]);
    }

    return dequeueCt;
}
