#include "FileCaching.h"

#include <atlbase.h>
#include <cassert>

#include <fstream>


#ifdef WIN32
#define stat _stat
#endif

std::wstring widenString(std::string s);

bool FileCaching::fileExists(std::string_view assetPath)
{
return FileCaching::fileExists(widenString(std::string (assetPath)));
}

bool FileCaching::fileExists(std::wstring_view assetPath)
{
    struct stat buf;
    return (_wstat(assetPath.data(), &buf) == 0);
}

std::wstring widenString(std::string s)
{
    //
    return std::wstring(CA2W(std::string(s).c_str()));
}

void FileCaching::saveAssetChangedTime(std::string_view assetPath)
{
 FileCaching::saveAssetChangedTime(widenString(std::string(assetPath)));
}

void FileCaching::saveAssetChangedTime(std::wstring_view assetPath)
{
    struct stat result;
    if (_wstat(assetPath.data(), &result) != 0)
    {
         printf("Could not read shader file date");
         ;
    }

    auto modifiedTime = result.st_mtime;

    std::wstring assetTimePath = std::wstring(assetPath) + L".modified";

    int size = result.st_size / sizeof(byte);
    std::ofstream myFile(assetTimePath, std::ifstream::in | std::ifstream::out | std::ifstream::trunc);
    myFile << modifiedTime;

    myFile.close();
}


bool FileCaching::assetOutOfDate(std::string_view assetPath)
{
 return FileCaching::assetOutOfDate(widenString(std::string(assetPath)));
}

//TODO JS: Just checking date for now -- prefer some kind of hash
bool FileCaching::assetOutOfDate(std::wstring_view assetPath)
{
    //Last changed check 
    struct stat result;
    if (_wstat(assetPath.data(), &result) != 0)
    {
        printf("Could not read shader file date");
        assert(false);

    }

    auto modifiedTime = result.st_mtime;

    std::wstring assetTimePath = std::wstring(assetPath) + L".modified";


    if (_wstat(assetTimePath.c_str(), &result) != 0)
    {
        return true;
    }

    int size = result.st_size / sizeof(byte);
    std::ifstream myFile(assetTimePath, std::ifstream::in | std::ifstream::out);
    if (!myFile.is_open())
    {
        //Could not open shadertime file, assume new shader
        return true;
    }
    auto buffer = new char [size];

    myFile.read(buffer, size);

    long long savedTime = atoll(buffer);

    myFile.close();

    //TODO JS: update file with current time after compilation finishes

    if (savedTime == modifiedTime)
    {
        return false;
    }
}