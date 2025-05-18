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
void LoadTexturesThreaded_LoadCachedTextures(Allocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
    std::span<TextureCreation::LOAD_KTX_CACHED_args> textureCreationWork);
void LoadTexturesThreaded_CacheUnimportedTextures(Allocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureCreation::TextureImportProcessTemporaryTexture> threadInput,
    std::span<TextureCreation::LOAD_KTX_CACHED_args> threadOutput);

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
                          std::span<TextureCreation::TextureImportRequest> textureCreationWork)
{
    MemoryArena::memoryArena threadPoolAllocator{};
    initialize(&threadPoolAllocator, 64 * 100000);

    Array texturesToImport = MemoryArena::AllocSpan<TextureCreation::TextureImportRequest>(&threadPoolAllocator, textureCreationWork.size());
    Array newphase1Result = MemoryArena::AllocSpan<TextureCreation::TextureImportProcessTemporaryTexture>(&threadPoolAllocator, textureCreationWork.size());
    Array TexturesToLoadFromCache = MemoryArena::AllocSpan<TextureCreation::LOAD_KTX_CACHED_args>(&threadPoolAllocator, textureCreationWork.size());
    for(int i =0; i < textureCreationWork.size(); i++)
    {
        auto& w = textureCreationWork[i];
        switch (w.mode)
        {
        case TextureCreation::TextureImportMode::IMPORT_FILE:
        case TextureCreation::TextureImportMode::IMPORT_GLTF:
            //Add all of the textures which need to go through the frontend in order to be converted into TexturesToLoadFromCache
            texturesToImport.push_back(w);
            break;
        case TextureCreation::TextureImportMode::LOAD_KTX_CACHED:
            //Add all of the textures we can immediately load from cache -- later, the frontend job will add the rest of the textures.
            TexturesToLoadFromCache.push_back(w.addtlData.gltfCacheArgs);
            break;
        }
    }
    
    for(int i = 0; i < texturesToImport.size(); i++)
    {
        newphase1Result.push_back(TextureCreation::CreateTextureFromArgs_Start(mainThreadContext, texturesToImport[i])); //Do this synchronously for now, its fast and can be faster if I b uild the cbuffer
    }
    
    LoadTexturesThreaded_CacheUnimportedTextures(&threadPoolAllocator, mainThreadContext,newphase1Result.getSpan(), TexturesToLoadFromCache.getSubSpanToCapacity(TexturesToLoadFromCache.size()));

    LoadTexturesThreaded_LoadCachedTextures(&threadPoolAllocator, mainThreadContext, dstTextures, TexturesToLoadFromCache.getSubSpanToCapacity());
}

void LoadTexturesThreaded_CacheUnimportedTextures(Allocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureCreation::TextureImportProcessTemporaryTexture> threadInput,
    std::span<TextureCreation::LOAD_KTX_CACHED_args> threadOutput)
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
void LoadTexturesThreaded_LoadCachedTextures(Allocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
    std::span<TextureCreation::LOAD_KTX_CACHED_args> textureCreationWork)
{
    superLuminalAdd("Texture loading backend run");
    assert(textureCreationWork.size() == dstTextures.size());
    //Initialize a pool
    ThreadPool::Pool threadPool;
    size_t workItemCt = textureCreationWork.size();
    size_t threadCt = std::min(workItemCt, (size_t)6);
    InitializeThreadPool(threadPoolAllocator, &threadPool, workItemCt, threadCt);

    std::span<VkFence> fences = MemoryArena::AllocSpan<VkFence>(threadPoolAllocator, threadCt);
    auto step1Output = MemoryArena::AllocSpan<TextureCreation::TextureImportResult>(threadPoolAllocator, workItemCt);

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
    superLuminalEnd();
}

