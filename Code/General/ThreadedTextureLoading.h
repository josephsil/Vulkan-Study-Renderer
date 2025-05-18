#pragma once
#include <concurrentqueue.h>
#include <span>

#include "Renderer/PerThreadRenderContext.h"
struct TextureData;

namespace TextureCreation
{
    struct LOAD_KTX_CACHED_args;
    struct TextureImportProcessTemporaryTexture;
    struct TextureImportRequest;
    struct TextureImportResult;
}

struct TextureLoadingBackendJob
{
    PerThreadRenderContext MainThreadContext;
    std::span<TextureCreation::TextureImportResult> workItemOutput;
    std::span<TextureCreation::LOAD_KTX_CACHED_args> workItemInput;
    std::span<PerThreadRenderContext> PerThreadContexts;
    std::span<TextureData> FinalOutput;
    
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx);
    void ON_COMPLETE_FN ();
    unsigned long long READ_RESULTS_FN();
};

struct TextureLoadingFrontendJob
{
    PerThreadRenderContext MainThreadContext;
    std::span<TextureCreation::LOAD_KTX_CACHED_args> workItemOutput;
    std::span<TextureCreation::TextureImportProcessTemporaryTexture> workItemInput;
    std::span<PerThreadRenderContext> PerThreadContexts;
    
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx);
    void ON_COMPLETE_FN ();
    unsigned long long READ_RESULTS_FN();
};


void LoadTexturesThreaded(PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures, std::span<TextureCreation::TextureImportRequest> textureCreationWork);
