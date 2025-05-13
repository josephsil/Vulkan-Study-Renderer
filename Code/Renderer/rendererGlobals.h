#pragma once
#include <span>
#include <string>
#include <VkBootstrap.h>
#include <General/MemoryArena.h>

struct PerThreadRenderContext;
static constexpr size_t MAX_SHADOWCASTERS = 8;
static constexpr int CASCADE_CT = 6;
#define MAX_SHADOWMAPS (MAX_SHADOWCASTERS * 8)
static constexpr int MAX_CAMERAS = 1;
static constexpr int HIZDEPTH = 6;
static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
static constexpr int SWAPCHAIN_SIZE = 3;
static constexpr int MAX_DRAWINDIRECT_COMMANDS = 200000; //Draw commands per frmae
static constexpr int MAX_DRAWS_PER_PIPELINE = 2000; //whatever, probably could be dynamic, will fix later 
static constexpr int MAX_PIPELINES = 80; //whatever, probably could be dynamic, will fix later
static constexpr int MAX_RENDER_PASSES = 120; //whatever, probably could be dynamic, will fix later
constexpr VkFormat shadowFormat = VK_FORMAT_D16_UNORM;
constexpr uint32_t SHADOW_MAP_SIZE = 2048;

template <typename T>
std::span<T > CreatePerFrameCollection(MemoryArena::memoryArena* arena)
{
    return MemoryArena::AllocSpan<T >(arena, MAX_FRAMES_IN_FLIGHT);
}

void registerDebugUtilsFn(PFN_vkSetDebugUtilsObjectNameEXT ptr);
void setDebugObjectName(VkDevice device, VkObjectType type, std::string name, uint64_t handle);
