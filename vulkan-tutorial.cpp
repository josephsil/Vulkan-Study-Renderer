#include <cstdlib>
#include "glm/glm.hpp"
#include "engineGlobals.h"
#include "game.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "General/InputHandling.h"
#include "Renderer/meshData.h"
#include "Renderer/TextureData.h"
#include "Renderer/gltf/gltfLoading.h"
#include "Scene/Scene.h"
#include "Code/Renderer/MainRenderer/vulkanRenderer.h"
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
std::vector<VkDescriptorSet> ImGuiExposedTextures;

int main()
{
    printf("NEW ENTRY");
    
     // Setup Dear ImGui context
     IMGUI_CHECKVERSION();
     ImGui::CreateContext();
     ImGuiIO& io = ImGui::GetIO();
     io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
     io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
     ImGuiExposedTextures = {};
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch
    
    
    vulkanRenderer renderer = {};
    
    MemoryArena::memoryArena sceneArena = {};
    MemoryArena::initialize(&sceneArena, 3 * 1000000);
    Scene scene = {};
    InitializeScene(&sceneArena, &scene);
    Add_Scene_Content(renderer.getFullRendererContext(), renderer.AssetDataAndMemory, &scene);
    renderer.initializeRendererForScene(&scene);
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
void engineLoop(vulkanRenderer* renderer, Scene* scene)
{

    renderer->beforeFirstUpdate(); //TODO JS: Extract non-renderer stuff out
    while(!QUIT)
    {


        bool imguiCaptureMouse = false;
        updateImgui(&imguiCaptureMouse);
        InputHandler_Update(imguiCaptureMouse);
        scene->Update();
        renderer->Update(scene); //TODO JS: Extract non-renderer stuff out
        // renderer->cleanup();
    }

    renderer->cleanup();
    
}



