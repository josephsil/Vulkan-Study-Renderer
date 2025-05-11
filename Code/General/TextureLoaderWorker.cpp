#include "TextureLoaderWorker.h"

#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/CommandPoolManager.h"

void TextureLoaderThreadWorker::WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
{
    CommandBufferPoolQueue* poolPtr = &(PerThreadCommandBuffer[thread_idx]);
    switch (ThreadsInput[work_item_idx].mode)
    {
    case TextureCreation::TextureCreationMode::FILE:
        assert(!"Error");
        break;
    case TextureCreation::TextureCreationMode::GLTFCREATE:
        ThreadsInput[work_item_idx].args.gltfCreateArgs.commandbuffer = poolPtr; //todo js hacky
        break;
    case TextureCreation::TextureCreationMode::GLTFCACHED:
        ThreadsInput[work_item_idx].args.gltfCacheArgs.commandbuffer = poolPtr; //todo js hacky
        break;
    }
    ThreadsOutput[work_item_idx] = TextureCreation::CreateTextureFromArgs_Start( ThreadsInput[work_item_idx]);
    ResultsReadyAtIndex.enqueue(work_item_idx);
}

void TextureLoaderThreadWorker::ON_COMPLETE_FN()
{
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
        FinalOutput[indexReadyToRead] =  TextureCreation::CreateTextureFromArgsFinalize(ThreadsOutput[indexReadyToRead]);
        printf("Reading at...Index: %llu \n", indexReadyToRead);
    }

    return dequeueCt;
}
