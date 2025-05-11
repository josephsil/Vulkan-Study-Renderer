#pragma once
#include <concurrentqueue.h>
#include <span>

#include "Renderer/RendererContext.h"
#include "Renderer/VulkanIncludes/forward-declarations-renderer.h"

struct TextureData;

namespace TextureCreation
{
    struct TextureCreationInfoArgs;
    struct TextureCreationStep1Result;
}

struct TextureLoaderThreadWorker
{
    std::span<TextureCreation::TextureCreationStep1Result> ThreadsOutput;
    std::span<TextureCreation::TextureCreationInfoArgs> ThreadsInput;
    std::span<RendererContext> PerThreadContext;
    RendererContext MainThreadContext;
    std::span<TextureData> FinalOutput;
    std::span<size_t>  readbackBufferForQueue;
    moodycamel::ConcurrentQueue<size_t> ResultsReadyAtIndex;
    void WORKER_FN(size_t work_item_idx, uint8_t thread_idx);

    void ON_COMPLETE_FN ();

    unsigned long long READ_RESULTS_FN();
};
