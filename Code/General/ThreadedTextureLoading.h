#pragma once
#include <concurrentqueue.h>
#include <span>

#include "Renderer/PerThreadRenderContext.h"
struct TextureData;

namespace TextureCreation
{
    struct TextureCreationInfoArgs;
    struct TextureCreationStep1Result;
}

struct TextureLoadingJobContext
{
    PerThreadRenderContext MainThreadContext;
    std::span<TextureCreation::TextureCreationStep1Result> workItemInput;
    std::span<TextureCreation::TextureCreationInfoArgs> workItemOutput;
    std::span<PerThreadRenderContext> PerThreadContext;
    std::span<TextureData> FinalOutput;
    
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx);
    void ON_COMPLETE_FN ();
    unsigned long long READ_RESULTS_FN();
};


void LoadTexturesThreaded(PerThreadRenderContext handles, std::span<TextureData> dstTextures, std::span<TextureCreation::TextureCreationInfoArgs> textureCreationWork);
