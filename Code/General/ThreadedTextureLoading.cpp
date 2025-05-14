#include "ThreadedTextureLoading.h"

#include "ThreadPool.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/CommandPoolManager.h"
#include "Renderer/rendererGlobals.h"
#include "Renderer/MainRenderer/VulkanRendererInternals/RendererHelpers.h"

void TextureLoadingJobContext::WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
{
    workItemOutput[work_item_idx] = TextureCreation::CreateTextureFromArgs_Start( PerThreadContext[thread_idx], workItemInput[work_item_idx]);
}

void TextureLoadingJobContext::ON_COMPLETE_FN()
{
    for (size_t i = 0; i < workItemOutput.size(); i++)
    {
        FinalOutput[i] =  TextureCreation::CreateTextureFromArgsFinalize(MainThreadContext, workItemOutput[i]);
    }
    for (auto ctx : PerThreadContext)
    {
        ctx.threadDeletionQueue->FreeQueue();
    }
    printf("Completed\n");
}

unsigned long long TextureLoadingJobContext::READ_RESULTS_FN()
{
    return 0;
}

void LoadTexturesThreaded(PerThreadRenderContext handles, std::span<TextureData> dstTextures,
    std::span<TextureCreation::TextureCreationInfoArgs> textureCreationWork)
{
    
    MemoryArena::memoryArena threadPoolAllocator{};
    initialize(&threadPoolAllocator, 64 * 100000);
    
    //Initialize a pool
    ThreadPool::Pool threadPool;
    size_t workItemCt = textureCreationWork.size();
    size_t threadCt = std::min(workItemCt, (size_t)32);
    size_t countPerSubmit = 8;
    InitializeThreadPool(&threadPoolAllocator, &threadPool, workItemCt, threadCt);

    std::span<VkFence> fences = MemoryArena::AllocSpan<VkFence>(&threadPoolAllocator, threadCt);

    TextureLoadingJobContext ThreadWorker =
    {
        .MainThreadContext =  handles,
        .workItemOutput = MemoryArena::AllocSpan<TextureCreation::TextureCreationStep1Result>(&threadPoolAllocator, workItemCt),
        .workItemInput = textureCreationWork,
        .PerThreadContext = MemoryArena::AllocSpan<PerThreadRenderContext>(&threadPoolAllocator, threadCt),
        .FinalOutput = dstTextures,
    };
    
    //Confiure per thread context
    for (size_t i = 0; i < threadCt; i++)
    {
        ThreadWorker.PerThreadContext[i] = {handles}; //initialize the per thread context by copying the render context
        static_createFence(ThreadWorker.PerThreadContext[i].device, &ThreadWorker.PerThreadContext[i].ImportFence,
                           "Fence for thread", ThreadWorker.PerThreadContext[i].threadDeletionQueue);
        
        ThreadWorker.PerThreadContext[i].threadDeletionQueue = new RendererDeletionQueue(
            ThreadWorker.PerThreadContext[i].device, ThreadWorker.PerThreadContext[i].allocator);

        ThreadWorker.PerThreadContext[i].textureCreationcommandPoolmanager = MemoryArena::Alloc<CommandPoolManager>(
            &threadPoolAllocator);
        
        new(ThreadWorker.PerThreadContext[i].textureCreationcommandPoolmanager) CommandPoolManager(
            *ThreadWorker.PerThreadContext[i].vkbd, ThreadWorker.PerThreadContext[i].threadDeletionQueue);
        
        setDebugObjectName(ThreadWorker.PerThreadContext[i].device, VK_OBJECT_TYPE_COMMAND_POOL, "thread ktx pool",
                           (uint64_t)ThreadWorker.PerThreadContext[i].textureCreationcommandPoolmanager->commandPool);
    }
    
    //Job submisison
    auto JobDataWrapper = ThreadPool::ThreadWorkWrapper(&ThreadWorker);
    CreateThreads(&threadPool, JobDataWrapper);
    SubmitRequests(&threadPool, workItemCt);
    WaitForCompletion(&threadPool, JobDataWrapper);
}

