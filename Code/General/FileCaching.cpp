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

bool FileCaching::TryGetKTXCachedPath(ArenaAllocator arena, const char* path, std::span<char>& ktxPath)
{
    auto str_chars = strnlen_s(path, 1024) / sizeof(char);
	auto scratchMemory = GetScratchStringMemory();
    auto oldPathSubSpan = fmt::basic_string_view(path, str_chars - 4);
	fmt::format_to_n(scratchMemory.begin(), scratchMemory.size(), "{}.ktx\0", oldPathSubSpan);
    ktxPath = scratchMemory.subspan(0, str_chars); 
    return FileCaching::FileExists(std::string_view(ktxPath.data()));
    
    
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
