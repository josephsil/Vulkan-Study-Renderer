#include <cstdlib>
#include <General/GLM_IMPL.h>
#include "engineGlobals.h"
#include "game.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "General/InputHandling.h"
#include "Renderer/MeshCreation/meshData.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/gltf/gltfLoading.h"
#include "Scene/Scene.h"
#include "Code/Renderer/MainRenderer/VulkanRenderer.h"
#include "General/ThreadPool.h"
#include "General/PrototypeThreadWorker.h"
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

void engineLoop(VulkanRenderer* renderer, Scene* scene);
std::vector<VkDescriptorSet> ImGuiExposedTextures;

int main()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
    ImGuiExposedTextures = {};
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch


    VulkanRenderer renderer = {};

    MemoryArena::memoryArena sceneArena = {};
    initialize(&sceneArena, 3 * 1000000);
    Scene scene = {};
    InitializeScene(&sceneArena, &scene);
    Add_Scene_Content(renderer.getMainRendererContext(), renderer.AssetDataAndMemory, &scene);
    sceneCountData sceneData = {.objectCount = scene.ObjectsCount(), .subMeshCount = scene.objects.subMeshesCount, .lightCount = scene.lightCount, .lightTypes = scene.lightTypes.getSpan()};
    renderer.InitializeRendererForScene(sceneData );
    renderer.initializePipelines( scene.lightCount);
    engineLoop(&renderer, &scene);

    return EXIT_SUCCESS;
}

void updateImgui(bool* captureMouse)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    //some imgui UI to test
    //ImGui::ShowDemoWindow();
    ImGui::Begin("js");
    ImGui::Text("TEST TEST");

    *captureMouse = ImGui::GetIO().WantCaptureMouse;
    ImGui::End();
}

void engineLoop(VulkanRenderer* renderer, Scene* scene)
{
    renderer->BeforeFirstUpdate(); //TODO JS: Extract non-renderer stuff out
    while (!QUIT)
    {
        bool imguiCaptureMouse = false;
        updateImgui(&imguiCaptureMouse);
        InputHandler_Update(imguiCaptureMouse);
        scene->Update();
        
        renderer->Update(scene); //TODO JS: Extract non-renderer stuff out
        // renderer->cleanup();
    }

    renderer->Cleanup();
}
