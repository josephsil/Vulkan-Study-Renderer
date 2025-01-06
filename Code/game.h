#pragma once
//"game logic" in the sense of, code that's "content"

struct Scene;
class AssetManager;
struct RendererContext;
void Add_Scene_Content(RendererContext rendererContext, AssetManager* rendererData, Scene* scene);
