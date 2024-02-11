#include "Transforms.h"
#include <stack>

#include "SceneObjectData.h"

//TODO P0:
/*
 *Look up table for locals -- store indices on graph and objects, make table during flatten
 *- GetLocalMatrix(ID)
 *  {
 *  localId = LUT[ID]
 *  return localMatrix[localId_something][localId_something]
 *      }
 *World matrices that are paralell with locals 
 *Update code that calculates world by looping over locals
 *System where like, every frame we loop over local transforms to build global transforms, then feed them to gpu
 *Move old transforms to new system 
 * Test with existing pipeline 
 */
//TODO P1:
/*
 * Feed transforms from gltf
 * Maybe API to make a bunch of graph side changes and then rebuild flattened after
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

void addChild(localTransform* tgt,localTransform* child)
{
    tgt->children.push_back(child);
}

void rmChild(localTransform* tgt, localTransform* remove)
{
    tgt->children.erase(std::remove(tgt->children.begin(), tgt->children.end(), remove), tgt->children.end());
}

struct stackEntry
{
    int visited;
    std::span<localTransform*> entry;
};

std::vector<flT_lookup> LUT;

flatlocalTransform* lookupflt(uint64_t ID)
{
    auto lookup = LUT[ID];
    return  &localTransformHiearchy[lookup.depth][lookup.index];
}

//This is probably insane, i have a cold and can't remember how trees work
void flattenTransformHiearchy(std::span<localTransform*> roots, Scene* scene)
{
    localTransformHiearchy.clear();
    LUT.clear();
    LUT.resize(scene->objectsCount());
    localTransformHiearchy.push_back({});
    std::stack<stackEntry> queue = {};
    for(int i = 0; i < roots.size(); i++)
    {
        if (roots[i]->children.size() != 0)
            queue.push({0, std::span(roots[i]->children)});
        localTransformHiearchy[0].push_back({roots[i]->matrix, roots[i]->name, 0});
        LUT[roots[i]->ID] = {roots[i]->depth, (uint32_t)localTransformHiearchy[roots[i]->depth].size() -1 }; //look up from TFORM ID into flattened hiearchy

        while(!queue.empty())
        {
            stackEntry* t = &queue.top();
            //We're finished with this node, pop it off the stack
            if(t->visited >= t->entry.size())
            {
                queue.pop();
                continue;
            }
            int i = t->visited;
            //"Inner loop"
            if (t->entry[i]->children.size() != 0)
                queue.push({0, t->entry[i]->children});
            if (localTransformHiearchy.size() == t->entry[i]->depth)
            {
                localTransformHiearchy.push_back({});
            }
            //Create new flattened transform and lut entry
            localTransformHiearchy[t->entry[i]->depth]
                .push_back({t->entry[i]->matrix, t->entry[i]->name, (uint8_t)(localTransformHiearchy[t->entry[i]->depth -1].size() -1)});
            LUT[t->entry[i]->ID] = {t->entry[i]->depth, (uint32_t)localTransformHiearchy[t->entry[i]->depth].size() -1 }; //look up from TFORM ID into flattened hiearchy

            t->visited++;
        }
    }
}
