#include <cstdlib>
#include "glm/glm.hpp"
#include "engineGlobals.h"

#include "General/InputHandling.h"
#include "Renderer/meshData.h"
#include "Renderer/TextureData.h"
#include "Renderer/gltf/gltfLoading.h"
#include "Scene/Scene.h"
#include "Code/Renderer/vulkanRenderer.h"
//RENDERER TODOS:
/*
 * -Submeshes
 * -Split "Render Scene" (backing data) from "Scene" (Object Data)
 * -LODs
 *      -And compacted vert buffer for binding to final draw
 * -Finer grained culling
 *      -Depth? Clusters?
 */

//GENERAL TODOS
/*
 * -Dear IMGUI
 * -Physics?
 */

void engineLoop(vulkanRenderer* renderer, Scene* scene);


int main()
{
    printf("NEW ENTRY");
    vulkanRenderer app = {};

    MemoryArena::memoryArena sceneArena = {};
    MemoryArena::initialize(&sceneArena, 3 * 1000000);
    Scene scene = {};
    InitializeScene(&sceneArena,&scene);
    app.PrepareForScene(&scene);
    engineLoop(&app, &scene);

    return EXIT_SUCCESS;
}

void engineLoop(vulkanRenderer* renderer, Scene* scene)
{
    
    while(!QUIT)
    {
        InputHandler_Update();
        scene->Update();
        renderer->Update(scene); //TODO JS: Extract non-renderer stuff out 
    }

    renderer->cleanup();
    
}

