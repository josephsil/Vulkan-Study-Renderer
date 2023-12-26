#pragma once
#include <string_view>

namespace FileCaching
{

  
    void saveAssetChangedTime(std::string_view assetPath);
    bool fileExists(std::string_view assetPath);
    bool assetOutOfDate(std::string_view assetPath);
    
    void saveAssetChangedTime(std::wstring_view assetPath);
    bool fileExists(std::wstring_view assetPath);
    bool assetOutOfDate(std::wstring_view assetPath);
    bool compareAssetAge(std::wstring_view assetNewer, std::wstring_view assetOlder);

    void touchFile(std::wstring_view path);
 
    
};
