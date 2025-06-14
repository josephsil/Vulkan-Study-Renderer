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
    void SaveAssetChangedTime(std::string_view assetPath);
    bool FileExists(std::string_view assetPath);
    bool IsAssetOutOfDate(std::string_view assetPath);

    void SaveAssetChangedTime(std::wstring_view assetPath);
    bool FileExists(std::wstring_view assetPath);
    bool IsAssetOutOfDate(std::wstring_view assetPath);
    bool CompareAssetAge(std::wstring_view assetNewer, std::wstring_view assetOlder);

    void UpdateModified(std::wstring_view path);
};
