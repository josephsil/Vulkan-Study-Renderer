#pragma once
#include <span>
#include <string>
#include <VkBootstrap.h>
#include <General/MemoryArena.h>
static constexpr int WIDTH = (int)(1280 * 1.5);
static constexpr int HEIGHT = (int)(720 * 1.5);
static constexpr size_t MAX_SHADOWCASTERS = 8;

#define MAX_SHADOWMAPS (MAX_SHADOWCASTERS * 8)
#define MAX_SHADOWMAPS_WITH_CULLING (16)
static constexpr int MAX_CAMERAS = 1;
static constexpr int HIZDEPTH = 10;
static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
static constexpr int SWAPCHAIN_SIZE = 2;
static constexpr int MAX_DRAWINDIRECT_COMMANDS = 1000000; //Draw commands per frmae
static constexpr int MAX_DRAWS_PER_PIPELINE = 2000; //whatever, probably could be dynamic, will fix later 
static constexpr int MAX_PIPELINES = 80; //whatever, probably could be dynamic, will fix later
static constexpr int MAX_RENDER_PASSES = 120; //whatever, probably could be dynamic, will fix later
constexpr VkFormat shadowFormat = VK_FORMAT_D32_SFLOAT;
static constexpr size_t MESHLET_VERTICES = 64;
static constexpr size_t MESHLET_TRIS = 124;
static constexpr size_t MESHLET_INDICES =MESHLET_TRIS * 3;

template <typename T>
std::span<T > CreatePerFrameCollection(MemoryArena::memoryArena* arena)
{
    return MemoryArena::AllocSpan<T >(arena, MAX_FRAMES_IN_FLIGHT);
}

std::span<char> GetScratchMemory();
void registerDebugUtilsFn(PFN_vkSetDebugUtilsObjectNameEXT ptr);
void registerTransitionImagefn(PFN_vkTransitionImageLayoutEXT ptr);
void registerCopyImageToMemoryFn(PFN_vkCopyImageToMemoryEXT ptr);
void SetDebugObjectName(VkDevice device, VkObjectType type, char* name, uint64_t handle);
void SetDebugObjectNameS(VkDevice device, VkObjectType type, std::string name, uint64_t handle);
const char* GetDebugObjectName(uint64_t handle);
void vkTransitionImageLayout(
    VkDevice                                    device,
    uint32_t                                    transitionCount,
    const VkHostImageLayoutTransitionInfoEXT*   pTransitions);
    void vkCopyImageToMemory(VkDevice device, void*targetHostPointer, VkImage sourceImage,
    VkExtent3D extent, uint32_t mipLevel, uint32_t baseArrayLayer, uint32_t mipCt = 1, uint32_t layerCt = VK_REMAINING_ARRAY_LAYERS);

void registerSuperluminal();

void superLuminalAdd(const char* inID);
void superLuminalEnd();