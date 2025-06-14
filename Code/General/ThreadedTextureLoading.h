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

//Parallel texture loading
void LoadTexturesThreaded(PerThreadRenderContext mainThreadContext, std::span<TextureData> dstTextures, std::span<TextureCreation::TextureImportRequest> textureCreationWork);
