#include "ThreadedTextureLoading.h"

#include "ThreadPool.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/CommandPoolManager.h"
#include "Renderer/rendererGlobals.h"
#include "Renderer/MainRenderer/VulkanRendererInternals/RendererHelpers.h"

void TextureLoadingBackendJob::WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
{
    workItemOutput[work_item_idx] = TextureCreation::CreateTextureFromArgs_LoadCachedFiles( PerThreadContexts[thread_idx], workItemInput[work_item_idx]);
}

void TextureLoadingBackendJob::ON_COMPLETE_FN()
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

unsigned long long TextureLoadingBackendJob::READ_RESULTS_FN()
{
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
void LoadTexturesThreaded_Backend(Allocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
    std::span<TextureCreation::TextureCreationInfoArgs> textureCreationWork);
void LoadTexturesThreaded_Frontend(Allocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureCreation::TempTextureStepResult> threadInput,
    std::span<TextureCreation::TextureCreationInfoArgs> threadOutput);

void TextureLoadingFrontendJob::WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
{
    workItemOutput[work_item_idx] = TextureCreation::CreateTexture_Cache_Temp_To_KTX_Step( PerThreadContexts[thread_idx], workItemInput[work_item_idx]);
}

void TextureLoadingFrontendJob::ON_COMPLETE_FN()
{
    for (auto ctx : PerThreadContexts)
    {
        ctx.threadDeletionQueue->FreeQueue();
    }
}

unsigned long long TextureLoadingFrontendJob::READ_RESULTS_FN()
{
    return 0;
}

void LoadTexturesThreaded(PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
                          std::span<TextureCreation::TextureCreationInfoArgs> textureCreationWork)
{
    MemoryArena::memoryArena threadPoolAllocator{};
    initialize(&threadPoolAllocator, 64 * 100000);

    Array newphase1Input = MemoryArena::AllocSpan<TextureCreation::TextureCreationInfoArgs>(&threadPoolAllocator, textureCreationWork.size());
    Array newphase1Result = MemoryArena::AllocSpan<TextureCreation::TempTextureStepResult>(&threadPoolAllocator, textureCreationWork.size());
    Array newFinalPhase2Input = MemoryArena::AllocSpan<TextureCreation::TextureCreationInfoArgs>(&threadPoolAllocator, textureCreationWork.size());
    //WIP -- separate out 'new phase 2' stuff from the rest
    for(int i =0; i < textureCreationWork.size(); i++)
    {
        auto& w = textureCreationWork[i];
        switch (w.mode)
        {
        case TextureCreation::TextureCreationMode::FILE:
        case TextureCreation::TextureCreationMode::GLTFCREATE:
            newphase1Input.push_back(w);
            break;
        case TextureCreation::TextureCreationMode::KTXCACHED:
            newFinalPhase2Input.push_back(w);
            break;
        }
    }
    
    for(int i = 0; i < newphase1Input.size(); i++)
    {
        newphase1Result.push_back(TextureCreation::CreateTextureFromArgs_Start(mainThreadContext, newphase1Input[i])); //Do this synchronously for now, its fast and can be faster if I b uild the cbuffer
    }
    
    LoadTexturesThreaded_Frontend(&threadPoolAllocator, mainThreadContext,newphase1Result.getSpan(), newFinalPhase2Input.getSubSpanToCapacity(newFinalPhase2Input.size()));

    LoadTexturesThreaded_Backend(&threadPoolAllocator, mainThreadContext, dstTextures, newFinalPhase2Input.getSubSpanToCapacity());
}

void LoadTexturesThreaded_Frontend(Allocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureCreation::TempTextureStepResult> threadInput,
    std::span<TextureCreation::TextureCreationInfoArgs> threadOutput)
{
    assert(threadInput.size() == threadOutput.size());
    //Initialize a pool 
    ThreadPool::Pool threadPool;
    size_t workItemCt = threadOutput.size();
    size_t threadCt = std::min(workItemCt, (size_t)24);
    InitializeThreadPool(threadPoolAllocator, &threadPool, workItemCt, threadCt);

    TextureLoadingFrontendJob ThreadWorker =
   {
        .MainThreadContext =  mainThreadContext,
        .workItemOutput = threadOutput,
        .workItemInput = threadInput,
        .PerThreadContexts = MemoryArena::AllocSpan<PerThreadRenderContext>(threadPoolAllocator, threadCt),
    };
    createPerThreadRenderContexts(threadPoolAllocator, mainThreadContext, ThreadWorker.PerThreadContexts);

    auto JobDataWrapper = ThreadPool::ThreadWorkWrapper(&ThreadWorker);
    CreateThreads(&threadPool, JobDataWrapper);
    SubmitRequests(&threadPool, workItemCt);
    WaitForCompletion(&threadPool, JobDataWrapper);

    
}
void LoadTexturesThreaded_Backend(Allocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
    std::span<TextureCreation::TextureCreationInfoArgs> textureCreationWork)
{
    assert(textureCreationWork.size() == dstTextures.size());
    //Initialize a pool
    ThreadPool::Pool threadPool;
    size_t workItemCt = textureCreationWork.size();
    size_t threadCt = std::min(workItemCt, (size_t)1);
    InitializeThreadPool(threadPoolAllocator, &threadPool, workItemCt, threadCt);

    std::span<VkFence> fences = MemoryArena::AllocSpan<VkFence>(threadPoolAllocator, threadCt);
    auto step1Output = MemoryArena::AllocSpan<TextureCreation::TextureCreationStep1Result>(threadPoolAllocator, workItemCt);

    TextureLoadingBackendJob ThreadWorker =
    {
        .MainThreadContext =  mainThreadContext,
        .workItemOutput = step1Output,
        .workItemInput = textureCreationWork,
        .PerThreadContexts = MemoryArena::AllocSpan<PerThreadRenderContext>(threadPoolAllocator, threadCt),
        .FinalOutput = dstTextures,
    };

    //Confiure per thread context
    createPerThreadRenderContexts(threadPoolAllocator, mainThreadContext, ThreadWorker.PerThreadContexts);
   
    
    //Job submisison
    auto JobDataWrapper = ThreadPool::ThreadWorkWrapper(&ThreadWorker);
    CreateThreads(&threadPool, JobDataWrapper);
    SubmitRequests(&threadPool, workItemCt);
    WaitForCompletion(&threadPool, JobDataWrapper);
}

