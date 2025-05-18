#pragma once
#include <concurrentqueue.h>
#include <span>

#include "Renderer/PerThreadRenderContext.h"
struct TextureData;

namespace TextureCreation
{
    struct TempTextureStepResult;
    struct TextureCreationInfoArgs;
    struct TextureCreationStep1Result;
}

struct TextureLoadingBackendJob
{
    PerThreadRenderContext MainThreadContext;
    std::span<TextureCreation::TextureCreationStep1Result> workItemOutput;
    std::span<TextureCreation::TextureCreationInfoArgs> workItemInput;
    std::span<PerThreadRenderContext> PerThreadContexts;
    std::span<TextureData> FinalOutput;
    
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx);
    void ON_COMPLETE_FN ();
    unsigned long long READ_RESULTS_FN();
};

struct TextureLoadingFrontendJob
{
    PerThreadRenderContext MainThreadContext;
    std::span<TextureCreation::TextureCreationInfoArgs> workItemOutput;
    std::span<TextureCreation::TempTextureStepResult> workItemInput;
    std::span<PerThreadRenderContext> PerThreadContexts;
    
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx);
    void ON_COMPLETE_FN ();
    unsigned long long READ_RESULTS_FN();
};


void LoadTexturesThreaded(PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures, std::span<TextureCreation::TextureCreationInfoArgs> textureCreationWork);
