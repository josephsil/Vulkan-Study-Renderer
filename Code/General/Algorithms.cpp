#include "Algorithms.h"
void ReorderParallelOpaqueSpans(ArenaAllocator scratchMemory, 
							std::span<OpaqueSpan> targets, 
							std::span<size_t> indices)
{
	#if _DEBUG 
	size_t count = targets[0].count; 
	for (int i = 1; i < targets.size(); i++)
	{
		assert(targets[i].count == count);
	}
	#endif
	size_t max = 0; 
	for(auto& os : targets)
	{
		if (os.size > max)
		{
			max = os.size;
		}
	}
	//Allocate a big enough scratch space to store our largest type
	void* tempCopy = MemoryArena::alloc(scratchMemory, indices.size() * max);

	for (int i = 0; i < targets.size(); i++)
	{
		ptrdiff_t valueSize = targets[i].size;
		memcpy(tempCopy, targets[i].data, targets[i].count * valueSize); //Copy unsorted version to temp memory 
		for (int j = 0; j < targets[i].count; j++)
		{
			ptrdiff_t nextValue = valueSize * j;
			ptrdiff_t reorderedValue = valueSize * indices[j];
			memcpy((char*)targets[i].data + reorderedValue, (char*)tempCopy + nextValue, valueSize);
		}

	}

}