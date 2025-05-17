#include "ThreadedTextureLoading.h"

#include "ThreadPool.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/CommandPoolManager.h"
#include "Renderer/rendererGlobals.h"
#include "Renderer/MainRenderer/VulkanRendererInternals/RendererHelpers.h"

void TextureLoadingJobContext::WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
{
    workItemOutput[work_item_idx] = TextureCreation::CreateTextureFromArgs_Start( PerThreadContexts[thread_idx], workItemInput[work_item_idx]);
}

void TextureLoadingJobContext::ON_COMPLETE_FN()
{
    for (size_t i = 0; i < workItemOutput.size(); i++)
    {
        FinalOutput[i] =  TextureCreation::CreateTextureFromArgsFinalize(MainThreadContext, workItemOutput[i]);
    }
    for (auto ctx : PerThreadContexts)
    {
        ctx.threadDeletionQueue->FreeQueue();
    }
    printf("Completed\n");
}

unsigned long long TextureLoadingJobContext::READ_RESULTS_FN()
{
    //assert(!"Unimplemented!");
    return 0;
}

void createPerThreadRenderContexts(Allocator allocator, PerThreadRenderContext mainThreadContext, std::span<PerThreadRenderContext> newContexts)
{
    for (auto& newctx : newContexts)
    {
        newctx = {mainThreadContext}; //initialize the per thread context by copying the render context
        newctx.threadDeletionQueue = new RendererDeletionQueue(
           newctx.device, newctx.allocator);
        static_createFence(newctx.device, &newctx.ImportFence, "Fence for thread", newctx.threadDeletionQueue);
        newctx.textureCreationcommandPoolmanager = MemoryArena::Alloc<CommandPoolManager>(allocator);
        new(newctx.textureCreationcommandPoolmanager) CommandPoolManager(*mainThreadContext.vkbd, newctx.threadDeletionQueue);
        SetDebugObjectName(newctx.device, VK_OBJECT_TYPE_COMMAND_POOL, "thread ktx pool",(uint64_t)newctx.textureCreationcommandPoolmanager->commandPool);
    }
}

void LoadTexturesThreaded(PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
    std::span<TextureCreation::TextureCreationInfoArgs> textureCreationWork)
{
    
    MemoryArena::memoryArena threadPoolAllocator{};
    initialize(&threadPoolAllocator, 64 * 100000);
    
    //Initialize a pool
    ThreadPool::Pool threadPool;
    size_t workItemCt = textureCreationWork.size();
    size_t threadCt = std::min(workItemCt, (size_t)32);
    InitializeThreadPool(&threadPoolAllocator, &threadPool, workItemCt, threadCt);

    std::span<VkFence> fences = MemoryArena::AllocSpan<VkFence>(&threadPoolAllocator, threadCt);

    TextureLoadingJobContext ThreadWorker =
    {
        .MainThreadContext =  mainThreadContext,
        .workItemOutput = MemoryArena::AllocSpan<TextureCreation::TextureCreationStep1Result>(&threadPoolAllocator, workItemCt),
        .workItemInput = textureCreationWork,
        .PerThreadContexts = MemoryArena::AllocSpan<PerThreadRenderContext>(&threadPoolAllocator, threadCt),
        .FinalOutput = dstTextures,
    };

    //Confiure per thread context
    createPerThreadRenderContexts(&threadPoolAllocator, mainThreadContext, ThreadWorker.PerThreadContexts);
   
    
    //Job submisison
    auto JobDataWrapper = ThreadPool::ThreadWorkWrapper(&ThreadWorker);
    CreateThreads(&threadPool, JobDataWrapper);
    SubmitRequests(&threadPool, workItemCt);
    WaitForCompletion(&threadPool, JobDataWrapper);
}

