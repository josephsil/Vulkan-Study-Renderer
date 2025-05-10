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
#include "General/ImportThreads.h"
#include "General/Mock_prototype_threads_impl.h"
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

    ImportThreads t;
    printf("THREADS \n");
    MemoryArena::memoryArena threadArena  {};
    initialize(&threadArena, 3 * 1000000);

    MemoryArena::memoryArena differentArena  {};
    initialize(&differentArena, 3 * 1000000);
    std::span<testThreadWorkData> workData = MemoryArena::AllocSpan<testThreadWorkData>(&differentArena, 1200);
    int i = 0;
    for (auto& work_data : workData)
    {
        work_data.requestdata = i++;
    }
    InitializeThreadPool(&threadArena, &t, workData.data(), sizeof(testThreadWorkData), workData.size());
    workerContext testThreadWorkerContext =
        {
            .contextdata = MemoryArena::AllocSpan<int>(&differentArena, 10).data(), // 
            .threadWorkerFunction = [](workerContext* _this, void* data, uint8_t thread_idx)
            {
                testThreadWorkData* d = (testThreadWorkData*)data; 
                *d = {thread_idx};
            },
        .completeJobFN = [] (workerContext* _this, void* data)
        {
            auto reqspan = std::span((testThreadWorkData*)data, 1200);
            auto dstSpan =  std::span((int*)_this->contextdata, 10);
            for (auto& result :reqspan)
            {
                printf("%d == \n", result.requestdata);
                dstSpan[result.requestdata] += 1;
            }
            int j = 0;
            size_t ct = 0;
            for (auto result : dstSpan)
            {
                printf("%d: %d  ", j, result);
                j++;
                ct += result;
            }
            printf("\n %llu \n", ct);
        }
        };

    auto wct = workerContext3();
    CreateThreads(&t,testThreadWorkerContext );

    SubmitRequests(&t, workData.size());
    auto r = WaitForCompletion(&t, testThreadWorkerContext);
    auto code = r ? 0 : -100;

    printf("NEW ENTRY \n");
    exit(code);

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
    Add_Scene_Content(renderer.getFullRendererContext(), renderer.AssetDataAndMemory, &scene);
    sceneCountData sceneData = {.objectCount = scene.objectsCount(), .lightCount = scene.lightCount, .lightTypes = scene.lightTypes.getSpan()};
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
