#include <cstdlib>
#include <General/GLM_impl.h>
#include "engineGlobals.h"
#include "game.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "General/InputHandling.h"
#include "Renderer/MeshCreation/MeshData.h"
#include "Renderer/TextureCreation/TextureData.h"
#include "Renderer/gltf/gltfLoading.h"
#include "Scene/Scene.h"
#include "Code/Renderer/MainRenderer/VulkanRenderer.h"
#include "General/ThreadPool.h"
#include "General/PrototypeThreadWorker.h"


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


    VulkanRenderer renderer;
    MemoryArena::Allocator sceneArena = {};
    MemoryArena::Initialize(&sceneArena, 3 * 1000000);
    Scene scene;
    InitializeScene(&sceneArena, &scene);

    Add_Scene_Content(renderer.GetMainRendererContext(), renderer.AssetDataAndMemory, &scene);

    auto meshletCt = VulkanRenderer::CalculateTotalDrawCount(&scene, renderer.AssetDataAndMemory->meshData.perSubmeshData.getSpan());
    
	SceneSizeData sceneData = {.objectCount = scene.GetObjectsCount(), .totalDrawCt = meshletCt, .lightCount = scene.lightCount, .lightTypes = scene.lightTypes.getSpan()};
    renderer.InitializeRendererForScene(sceneData );
    renderer.InitializePipelines( scene.lightCount);


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
    renderer->BeforeFirstUpdate(); 
    while (!QUIT)
    {
        bool imguiCaptureMouse = false;
        updateImgui(&imguiCaptureMouse);
        InputHandler_Update(imguiCaptureMouse);
        scene->Update();
        
        renderer->Update(scene); 
    }

    renderer->Cleanup();
}
