#pragma once
//"game logic" in the sense of, code that's "content"

struct Scene;
class AssetManager;
struct PerThreadRenderContext;
void Add_Scene_Content(PerThreadRenderContext rendererContext, AssetManager* rendererData, Scene* scene);
