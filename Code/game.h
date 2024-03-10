#pragma once
//"game logic" in the sense of, code that's "content"

struct Scene;
class RendererSceneData;
struct RendererContext;
void Add_Scene_Content(RendererContext rendererContext, RendererSceneData* rendererData, Scene* scene);
