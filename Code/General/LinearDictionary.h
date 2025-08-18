#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

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
		
		if constexpr (std::is_same_v<char*, K>)
		{
			return std::strcmp(const_cast<char*>(key), const_cast<char*>(rhskey)) == 0;
		}
		else 
		{
			return key == rhskey;
		}
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

	void Clear()
	{

		memset(keys,0, sizeof(keys));
		memset(values,0, sizeof(values));
	}
};


