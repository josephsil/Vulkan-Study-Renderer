#pragma once
#include <string_view>

#include "MemoryArena.h"



//Used by asset importers, checks for cached versions of files and
//Handles tracking when cached files are out of date
//Older code, needs cleanup
//The caching strategy for textures is also not great -- should be directly caching kvbuffer/vkimages
namespace FileCaching
{
	constexpr char cacheImageSuffix[13] = ".ktx.cached\0";
	uint32_t GetCacheImageSuffixLen();
	std::span<char> GetImagePathWithCacheSuffix(char* sourcePath, std::span<char> target);
    bool TryGetKTXCachedPath(ArenaAllocator arena, const char* path, std::span<char>& ktxPath);
    void SaveAssetChangedTime_Narrow(std::string assetPath);
    bool FileExists(std::string_view assetPath);
    bool IsAssetOutOfDate_Narrow(std::string_view assetPath);

    void SaveAssetChangedTime(platform_stringview assetPath);
    bool FileExists(platform_string assetPath);
    bool IsAssetOutOfDate(platform_stringview assetPath);
    bool CompareAssetAge(platform_stringview assetNewer, platform_stringview assetOlder);

    void UpdateModified(platform_stringview path);
};
