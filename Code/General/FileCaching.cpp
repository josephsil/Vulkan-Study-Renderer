#include "FileCaching.h"


#include <cassert>
#include <fstream>
#include <iostream>

#include <fmtInclude.h>
#include <Renderer/rendererGlobals.h>//

//Platform specific time stuff. TODO: move somewhere and deduplicate




//Needs rewrite, this was one of my first 'learning cpp' files

std::string WidenString(std::string s);

bool FileCaching::FileExists(std::string_view assetPath)
{
    return FileExists(WidenString(std::string(assetPath)));
}

bool FileCaching::FileExists(platform_string assetPath)
{
    struct stat buf;
    return (STAT(assetPath.data(), &buf) == 0);
}

#ifdef _WIN32

const wchar_t* suffix = L".modified";
#else

const char* suffix = ".modified";
#endif

size_t _strlen(char* p)
{
#ifdef _WIN32
return strnlen_s(p, 1024) / sizeof(char); //-1 for null terminator
#else
return strnlen(p, 1024) / sizeof(char);
#endif
}
#ifdef _WIN32
std::wstring WidenString(std::string s)
{
    return std::wstring(CA2W(std::string(s).c_str()));
}
#else
std::string WidenString(std::string s)
{
    return std::string(s);
}
#endif
uint32_t FileCaching::GetCacheImageSuffixLen()
{
	return sizeof(FileCaching::cacheImageSuffix);

}
std::span<char> FileCaching::GetImagePathWithCacheSuffix(char* sourcePath, std::span<char> targetMemory)
{
    
	auto str_chars = _strlen(sourcePath); //-1 for null terminator
	auto oldPathSpan = fmt::basic_string_view(sourcePath, str_chars);
	uint32_t _size = (uint32_t)fmt::format_to_n(targetMemory.begin(), targetMemory.size(), "{}{}", 
												oldPathSpan, cacheImageSuffix).size;
    return targetMemory.subspan(0, _size); 
}

bool IsCachedKTX(std::span<char> sourcePath)
{
	auto suffixlen = FileCaching::GetCacheImageSuffixLen();
	auto subspan = sourcePath.subspan(sourcePath.size() - suffixlen);
	if (std::string_view(subspan.data(),subspan.size()) == std::string_view(FileCaching::cacheImageSuffix, suffixlen))
	{
		return true;
	}
	return false;
}

bool FileCaching::TryGetKTXCachedPath(ArenaAllocator arena, const char* path, std::span<char>& ktxPath)
{
	char* _path = const_cast<char*>(path);
	auto str_chars = _strlen(_path);
	std::span<char> cachedKTXPath =  std::span(_path, str_chars);
	if (!IsCachedKTX({_path, str_chars}))
	{
		auto newPath = FileCaching::GetImagePathWithCacheSuffix(_path, std::span<char>(&GetScratchStringMemory()[0], 256));
		cachedKTXPath = MemoryArena::AllocCopySpan(arena, newPath);
	}
	assert(cachedKTXPath.size() != 0);
	ktxPath = cachedKTXPath;
    return FileCaching::FileExists(std::string_view(cachedKTXPath.data()));
    
    
}

void FileCaching::SaveAssetChangedTime_Narrow(std::string assetPath)
{
    SaveAssetChangedTime(WidenString((assetPath)));
}

void FileCaching::SaveAssetChangedTime(platform_stringview assetPath)
{
    struct stat result;
    if (STAT(assetPath.data(), &result) != 0)
    {
        assert(!"Could not read asset file date");
    }

    auto modifiedTime = result.st_mtime;


    //TODO: Better modified path, which we can copy separately in build step (rather than placing modified files adjacent to source files)
    platform_string assetTimePath = platform_string(assetPath) + suffix;

    size_t size = result.st_size / sizeof(std::byte);
    std::ofstream myFile(assetTimePath, std::ifstream::in | std::ifstream::out | std::ifstream::trunc);
    myFile << modifiedTime;

    myFile.close();
}


bool FileCaching::IsAssetOutOfDate_Narrow(std::string_view assetPath)
{
    return IsAssetOutOfDate(WidenString(std::string(assetPath)));
}

long long ReadModifiedTime(platform_stringview assetPath)
{

    //TODO: Better modified path, which we can copy separately in build step (rather than placing modified files adjacent to source files)
    platform_string assetTimePath = platform_string(assetPath) + suffix;
    struct stat result;
    if (STAT(assetTimePath.data(), &result) != 0)
    {
        return true;
    }

    size_t size = result.st_size / sizeof(unsigned char);
    std::ifstream myFile(assetTimePath.data(), std::ifstream::in | std::ifstream::out);
    if (!myFile.is_open())
    {
        //Could not open modified file, assume new asset
        return true;
    }
    auto buffer = new char [size];

    myFile.read(buffer, size);

    long long savedTime = atoll(buffer);

    myFile.close();

    return savedTime;
}

bool FileCaching::CompareAssetAge(platform_stringview assetNewer, platform_stringview assetOlder)
{
    return ReadModifiedTime(assetNewer) > ReadModifiedTime(assetOlder);
}

void FileCaching::UpdateModified(platform_stringview path)
{
    utimbuf new_times;
    struct stat result;
    STAT(path.data(), &result);
    new_times.actime = result.st_atime;
    new_times.modtime = time(nullptr); //current time
    UTIME(path.data(), &new_times);
}

bool FileCaching::IsAssetOutOfDate(platform_stringview assetPath)
{
    assert(assetPath.data() != nullptr);
    //Last changed check 
    struct stat result;
    if (STAT(assetPath.data(), &result) != 0)
    {
        printf("Could not read shader file date");
        assert(false);
    }

    auto modifiedTime = result.st_mtime;


    long long savedTime = ReadModifiedTime(assetPath);

    if (savedTime == modifiedTime)
    {
        return false;
    }

    return true;
}
