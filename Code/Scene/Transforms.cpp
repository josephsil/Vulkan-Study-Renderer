#include "Transforms.h"
#include <stack>

#include "AssetManager.h"
#include "General/MemoryArena.h"

//TODO P0:
/*
 *DONE Look up table for locals -- store indices on graph and objects, make table during flatten
 *- GetLocalMatrix(ID)
 *  {
 *  localId = LUT[ID]
 *  return localMatrix[localId_something][localId_something]
 *      }
 *DONE World matrices that are paralell with locals 
 *DONE Update code that calculates world by looping over locals
 *PROTOTYPE System where like, every frame we loop over local transforms to build global transforms, then feed them to gpu
 *DONE Move old transforms to new system 
 *DONE Test with existing pipeline 
 */
//TODO P1:
/*
 * Feed transforms from gltf
 * Maybe API to make a bunch of graph side changes and then rebuild flattened after
 * API to get transform from object, etc.
 * Move out of globals
 * */
//TODO p2:
/*
 *Better memory layout (nested localtransforms sequential in memory)
 *Don't rebuild from graph, do adds/removes directly to flattened
 *Get rid of sharedptrs, use a local arena and mark dead stuff (for compaction?)comm
 */
// std::shared_ptr<localTransform> DEBUG_CREATE_CHILD(localTransform* tgt, std::string childName, uint64_t ID, glm::mat4 childMat)
// {
//     tgt->children.push_back(std::make_shared<localTransform>(childMat, childName,ID, tgt->depth +1u));
//     return tgt->children[tgt->children.size() - 1];
// }

void addChild(localTransform* tgt, localTransform* child)
{
    tgt->children.push_back(child);
}

void rmChild(localTransform* tgt, localTransform* remove)
{
    std::erase(tgt->children, remove);
}

struct stackEntry
{
    int visited;
    std::span<localTransform*> entry;
};


flatlocalTransform* objectTransforms::GetTransform(uint64_t ID)
{
    auto lookup = _transform_lookup[ID];
    return &_local_transform_hiearchy[lookup.depth][lookup.index];
}

void objectTransforms::UpdateWorldTransforms()
{
    uint32_t idx = 0;
    for (int i = 0; i < _local_transform_hiearchy.size(); i++)
    {
        for (int j = 0; j < _local_transform_hiearchy[i].size(); j++)
        {
            auto transform = _local_transform_hiearchy[i][j];
            if (i != 0)
            {
                worldMatrices[i][j] = worldMatrices[i - 1][transform.parent] * transform.matrix;
                worldUniformScales[i][j] = worldUniformScales[i - 1][transform.parent] * transform.uniformScale;
            }
            else
            {
                worldMatrices[i][j] = transform.matrix;
                worldUniformScales[i][j] =transform.uniformScale;
            }
            idx++;
        }
    }
}

//This is probably insane, i have a cold and can't remember how trees work
void objectTransforms::RebuildTransformDataFromNodes(MemoryArena::memoryArena* arena)
{
    printf("Repeatedly rebuilding transform data leaks memory! todo! \n"); //TODO JS

    _local_transform_hiearchy.clear();
    _transform_lookup.clear();
    _transform_lookup.resize(1200); //TODO JS
    _local_transform_hiearchy.push_back({});

    std::stack<stackEntry> queue = {};
    for (int i = 0; i < rootTransformsView.size(); i++)
    {
        if (rootTransformsView[i]->children.size() != 0)
            queue.push({0, std::span(rootTransformsView[i]->children)});
        _local_transform_hiearchy[0].push_back({rootTransformsView[i]->matrix, rootTransformsView[i]->uniformScale, rootTransformsView[i]->name, 0});
        _transform_lookup[rootTransformsView[i]->ID] = {
            rootTransformsView[i]->depth,
            static_cast<uint32_t>(_local_transform_hiearchy[rootTransformsView[i]->depth].size()) - 1
        }; //look up from TFORM ID into flattened hiearchy

        while (!queue.empty())
        {
            stackEntry* t = &queue.top();
            //We're finished with this node, pop it off the stack
            if (t->visited >= t->entry.size())
            {
                queue.pop();
                continue;
            }
            int i = t->visited;
            //"Inner loop"
            if (t->entry[i]->children.size() != 0)
                queue.push({0, t->entry[i]->children});
            if (_local_transform_hiearchy.size() == t->entry[i]->depth)
            {
                _local_transform_hiearchy.push_back({});
            }
            //Create new flattened transform and lut entry
            assert(t->entry[i]->depth > 0);
            _local_transform_hiearchy[t->entry[i]->depth]
                .push_back({
                    t->entry[i]->matrix, t->entry[i]->uniformScale, t->entry[i]->name,
                    static_cast<uint8_t>(_local_transform_hiearchy[t->entry[i]->depth - 1].size() - 1)
                });
            _transform_lookup[t->entry[i]->ID] = {
                t->entry[i]->depth, static_cast<uint32_t>(_local_transform_hiearchy[t->entry[i]->depth].size()) - 1
            }; //look up from TFORM ID into flattened hiearchy

            t->visited++;
        }
    }

    //Allocate memory for matrices -- TODO leaks the old ones
    //TODO JS MESHLET PERF -- cache coherency here? 
    for (int i = 0; i < _local_transform_hiearchy.size(); i++)
    {
        worldMatrices.push_back();
        worldMatrices[i] = MemoryArena::AllocSpan<glm::mat4>(arena, _local_transform_hiearchy[i].size());
        worldUniformScales.push_back();
        worldUniformScales[i] = MemoryArena::AllocSpan<float>(arena, _local_transform_hiearchy[i].size());
    }
}
