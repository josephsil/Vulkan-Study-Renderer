#pragma once
//"game logic" in the sense of, code that's "content"

struct Scene;
class RendererLoadedAssetData;
struct RendererContext;
void Add_Scene_Content(RendererContext rendererContext, RendererLoadedAssetData* rendererData, Scene* scene);
