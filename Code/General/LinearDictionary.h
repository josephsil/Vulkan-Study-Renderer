#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>

//TODO: Grab a dictionary/hashset library and replace this
const int dict_size = 2048;
template <class K, class V>
struct LinearDictionary
{
    size_t ct;
    K keys[dict_size];
    V values[dict_size];

    bool comp(K key, K rhskey)
    {
        return key == rhskey;
    }
    bool comp(const char* key, const char* rhskey)
    {
        return comp(const_cast<char*>(key), const_cast<char*>(rhskey));
    }

  
    void Push(K key, V  v)
    {
        for(int i =0; i < ct; i++)
        {
            auto existingKey = keys[i];
        
            if (comp(key, existingKey))
            {
                assert(!"Can't re-submit key");
            }
        }
        keys[ct] = key;
        values[ct++] = v;
        
    }
    
    V& Find(K key)
    {
        for(size_t i =0; i < ct; i++)
        {
            auto existingKey = keys[i];
        
            if (comp(key, existingKey))
            {
                return values[i];
            }
        }
        assert(!"Key not found!");
        return values[0]; //Intentionally behind assert!
    }
};


