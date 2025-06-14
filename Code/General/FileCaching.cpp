#include "FileCaching.h"

#include <atlbase.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sys/utime.h>
#include <fmtInclude.h>
#include <Renderer/rendererGlobals.h>//

#ifdef WIN32
#define stat _stat
#endif


//Needs rewrite, this was one of my first 'learning cpp' files

std::wstring WidenString(std::string s);
bool FileCaching::FileExists(std::string_view assetPath)
{
    return FileExists(WidenString(std::string(assetPath)));
}

bool FileCaching::FileExists(std::wstring_view assetPath)
{
    struct stat buf;
    return (_wstat(assetPath.data(), &buf) == 0);
}

std::wstring WidenString(std::string s)
{
    return std::wstring(CA2W(std::string(s).c_str()));
}
uint32_t FileCaching::GetCacheImageSuffixLen()
{
	return sizeof(FileCaching::cacheImageSuffix);

}
std::span<char> FileCaching::GetImagePathWithCacheSuffix(char* sourcePath, std::span<char> targetMemory)
{
	auto str_chars = (strnlen_s(sourcePath, 1024) / sizeof(char)); //-1 for null terminator 
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
	auto str_chars = strnlen_s(_path, 1024) / sizeof(char);
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

void FileCaching::SaveAssetChangedTime(std::string_view assetPath)
{
    SaveAssetChangedTime(WidenString(std::string(assetPath)));
}

void FileCaching::SaveAssetChangedTime(std::wstring_view assetPath)
{
    struct stat result;
    if (_wstat(assetPath.data(), &result) != 0)
    {
        assert(!"Could not read asset file date");
    }

    auto modifiedTime = result.st_mtime;

    std::wstring assetTimePath = std::wstring(assetPath) + L".modified";

    int size = result.st_size / sizeof(byte);
    std::ofstream myFile(assetTimePath, std::ifstream::in | std::ifstream::out | std::ifstream::trunc);
    myFile << modifiedTime;

    myFile.close();
}


bool FileCaching::IsAssetOutOfDate(std::string_view assetPath)
{
    return IsAssetOutOfDate(WidenString(std::string(assetPath)));
}

long long ReadModifiedTime(std::wstring_view assetPath)
{
    std::wstring assetTimePath = std::wstring(assetPath) + L".modified";
    struct stat result;
    if (_wstat(assetTimePath.data(), &result) != 0)
    {
        return true;
    }

    int size = result.st_size / sizeof(byte);
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

bool FileCaching::CompareAssetAge(std::wstring_view assetNewer, std::wstring_view assetOlder)
{
    return ReadModifiedTime(assetNewer) > ReadModifiedTime(assetOlder);
}

void FileCaching::UpdateModified(std::wstring_view path)
{
    _utimbuf new_times;
    struct stat result;
    _wstat(path.data(), &result);
    new_times.actime = result.st_atime;
    new_times.modtime = time(nullptr); //current time
    _wutime(path.data(), &new_times);
}

bool FileCaching::IsAssetOutOfDate(std::wstring_view assetPath)
{
    assert(assetPath.data() != nullptr);
    //Last changed check 
    struct stat result;
    if (_wstat(assetPath.data(), &result) != 0)
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
