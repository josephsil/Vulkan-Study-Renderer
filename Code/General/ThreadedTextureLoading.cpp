#include "ThreadedTextureLoading.h"

#include "ThreadPool.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/CommandPoolManager.h"
#include "Renderer/rendererGlobals.h"
#include "Renderer/MainRenderer/VulkanRendererInternals/RendererHelpers.h"

//This spins up a few parallel jobs to import textures.
//Texture import is a janky multi step process currently - textures are imported from multiple formats,
//Mipmapped,
//Copied back to cpu ktx files,
//Compressed (via ktx lib)
//Saved to disk
//And then loaded from disk.
//This needs a general rethink, but more immediately I should directly cache the VKImage files rather than .ktx files on disk.
//TODO: Known bug to fix here, first load doesn't properly respect certain texture settings 
//See ThreadPool.h for context on these job structs and the pattern here
struct TextureLoadingLoadCachedTextureJob
{
    PerThreadRenderContext MainThreadContext;
    std::span<TextureCreation::TextureImportResult> workItemOutput;
    std::span<TextureCreation::LOAD_KTX_CACHED_args> workItemInput;
    std::span<PerThreadRenderContext> PerThreadContexts;
    std::span<TextureData> FinalOutput;
    
   
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
    {
        workItemOutput[work_item_idx] = TextureCreation::LoadAndUploadTextureFromImportCache( PerThreadContexts[thread_idx], workItemInput[work_item_idx]);
    }

    void ON_COMPLETE_FN()
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

    unsigned long long READ_RESULTS_FN()
    {
        return 0;
    }
};

struct TextureLoadingCreateTextureCacheJob
{
    PerThreadRenderContext MainThreadContext;
    std::span<TextureCreation::LOAD_KTX_CACHED_args> workItemOutput;
    std::span<TextureCreation::TextureImportProcessTemporaryTexture> workItemInput;
    std::span<PerThreadRenderContext> PerThreadContexts;
    
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx)
    {
        workItemOutput[work_item_idx] = TextureCreation::CreateTexture_Cache_Temp_To_KTX_Step( PerThreadContexts[thread_idx], workItemInput[work_item_idx]);
    }

    void ON_COMPLETE_FN()
    {
        for (auto ctx : PerThreadContexts)
        {
            ctx.threadDeletionQueue->FreeQueue();
        }
    }

    unsigned long long READ_RESULTS_FN()
    {
        return 0;
    }
};


//This is a kinda hacky thing until I generalize the multithreading code
//Copies the main thread context and overwrites the thread specific settings
void createPerThreadRenderContexts(ArenaAllocator allocator, PerThreadRenderContext mainThreadContext, std::span<PerThreadRenderContext> newContexts)
{
    for (auto& newctx : newContexts)
    {
        newctx = {mainThreadContext}; //initialize the per thread context by copying the render context
		newctx.threadDeletionQueue = MemoryArena::AllocDefaultConstructor<RendererDeletionQueue>(allocator); 
		InitRendererDeletionQueue(newctx.threadDeletionQueue,
           newctx.device, newctx.allocator);
        CreateFence(newctx.device, &newctx.ImportFence, "Fence for thread", newctx.threadDeletionQueue);
        newctx.textureCreationcommandPoolmanager = MemoryArena::AllocConstruct<CommandPoolManager>(allocator, *mainThreadContext.vkbd, newctx.threadDeletionQueue);
        SetDebugObjectNameS(newctx.device, VK_OBJECT_TYPE_COMMAND_POOL, "thread ktx pool",(uint64_t)newctx.textureCreationcommandPoolmanager->commandPool);
    }
}


void LoadTexturesThreaded_LoadCachedTextures(ArenaAllocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
    std::span<TextureCreation::LOAD_KTX_CACHED_args> textureCreationWork);
void LoadTexturesThreaded_ImportNewtexturesToCache(ArenaAllocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureCreation::TextureImportProcessTemporaryTexture> threadInput,
    std::span<TextureCreation::LOAD_KTX_CACHED_args> threadOutput);

void LoadTexturesThreaded(PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
                          std::span<TextureCreation::TextureImportRequest> textureCreationWork)
{
    MemoryArena::Allocator threadPoolAllocator{};
    MemoryArena::Initialize(&threadPoolAllocator, 64 * 10000);
    Array texturesToImport = MemoryArena::AllocSpan<TextureCreation::TextureImportRequest>(&threadPoolAllocator, textureCreationWork.size());
    Array temporaryTextureToCache = MemoryArena::AllocSpan<TextureCreation::TextureImportProcessTemporaryTexture>(&threadPoolAllocator, textureCreationWork.size());
    Array LoadCachedTexturesInput = MemoryArena::AllocSpan<TextureCreation::LOAD_KTX_CACHED_args>(&threadPoolAllocator, textureCreationWork.size());
    for(int i =0; i < textureCreationWork.size(); i++)
    {
        auto& w = textureCreationWork[i];
        switch (w.mode)
        {
        case TextureCreation::TextureImportMode::IMPORT_FILE:
        case TextureCreation::TextureImportMode::IMPORT_GLTF:
            //Gather all of the textures which need to have cached versions created 
            texturesToImport.push_back(w);
            break;
        case TextureCreation::TextureImportMode::LOAD_KTX_CACHED:
            //Add all of the textures we can immediately load from cache -- later, the frontend job will add the rest of the textures.
            LoadCachedTexturesInput.push_back(w.addtlData.gltfCacheArgs);
            break;
        }
    }
    
    for(int i = 0; i < texturesToImport.size(); i++)
    {
        temporaryTextureToCache.push_back(TextureCreation::GetTemporaryTextureDataForImportCache(mainThreadContext, texturesToImport[i])); //Do this synchronously for now, its fast and can be faster if I b uild the cbuffer
    }
    
    LoadTexturesThreaded_ImportNewtexturesToCache(&threadPoolAllocator, mainThreadContext,temporaryTextureToCache.getSpan(), LoadCachedTexturesInput.getSubSpanToCapacity(LoadCachedTexturesInput.size()));

    LoadTexturesThreaded_LoadCachedTextures(&threadPoolAllocator, mainThreadContext, dstTextures, LoadCachedTexturesInput.getSubSpanToCapacity());
}

void LoadTexturesThreaded_ImportNewtexturesToCache(ArenaAllocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureCreation::TextureImportProcessTemporaryTexture> threadInput,
    std::span<TextureCreation::LOAD_KTX_CACHED_args> threadOutput)
{
    assert(threadInput.size() == threadOutput.size());
    //Initialize a pool 
    ThreadPool::Pool threadPool;
    size_t workItemCt = threadOutput.size();
    size_t threadCt = std::min(workItemCt, (size_t)24);
    InitializeThreadPool(threadPoolAllocator, &threadPool, workItemCt, threadCt);

    TextureLoadingCreateTextureCacheJob ThreadWorker =
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
void LoadTexturesThreaded_LoadCachedTextures(ArenaAllocator threadPoolAllocator, PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures,
    std::span<TextureCreation::LOAD_KTX_CACHED_args> textureCreationWork)
{
    superLuminalAdd("Texture loading Load Cached Textures");
    assert(textureCreationWork.size() == dstTextures.size());
    //Initialize a pool
    ThreadPool::Pool threadPool;
    size_t workItemCt = textureCreationWork.size();
    size_t threadCt = std::min(workItemCt, (size_t)6);
    InitializeThreadPool(threadPoolAllocator, &threadPool, workItemCt, threadCt);

    std::span<VkFence> fences = MemoryArena::AllocSpan<VkFence>(threadPoolAllocator, threadCt);
    auto step1Output = MemoryArena::AllocSpan<TextureCreation::TextureImportResult>(threadPoolAllocator, workItemCt);

    TextureLoadingLoadCachedTextureJob ThreadWorker =
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

