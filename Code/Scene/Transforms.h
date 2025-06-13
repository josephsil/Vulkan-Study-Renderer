#pragma once

#include <memory>
#include <queue>
#include <span>
#include <string>
#include <vector>
#include <General/GLM_impl.h>

#include "General/Array.h"
#include "General/MemoryArena.h"


class AssetManager;
//Basic idea: loop over each level of the hiearchy and compute all the matrices
//In theory each level could be done in parallel
//so like
// [0][0][0][0][0][0][0][0][1][1][1][2][2][2][2][3][3][4]
//Within each sub-level I'll order them by common parent, like
// [1a][1a][1b][2a][2b][2c][2c][3a][3a][4a]
// That ought to give us cache hits looking to parent, right, which will help in the case of big flat levels
//All the 0s have no dependencies -- all the 1s depend on one of the 0s, and so on

//Tree representation used for building flat representation
//Going to replace with the flat transform eventually 
struct localTransform
{
    glm::mat4 matrix;
    float uniformScale;
    std::string name;
    size_t ID;
    uint8_t depth;
    std::vector<localTransform*> children;
};

std::shared_ptr<localTransform> DEBUG_CREATE_CHILD(localTransform* tgt, std::string childName, uint64_t ID,
                                                   glm::mat4 childMat);
void addChild(localTransform* tgt, localTransform* child);
void rmChild(localTransform* tgt, localTransform* remove);


//Flat representation for updating

//Non node stuff
struct flT_lookup
{
    uint8_t depth;
    uint32_t index;
};

struct flatlocalTransform
{
    glm::mat4 matrix;
    float uniformScale;
    std::string name;
    uint8_t parent; // Parent is always one level up 
};

//Data the scene uses for transforms
struct objectTransforms
{
    //Node based representaiton
    //Split out? Remove?
    std::vector<localTransform> transformNodes;
    Array<localTransform*> rootTransformsView;


    //world matrices we upload to gpu
    //Add validation to make sure they've been updated?
    Array<std::span<glm::mat4>> worldMatrices;
    Array<std::span<float>> worldUniformScales;

    //Representation we use every frame to calc transforms
    //TODO: Flatten more, use contiguous memory
    std::vector<std::vector<flatlocalTransform>> _local_transform_hiearchy; // [depth][object]


    flatlocalTransform* GetTransform(uint64_t ID);
    //used by get
    std::vector<flT_lookup> _transform_lookup;

    void set(uint64_t ID, glm::mat4 mat);
    void UpdateWorldTransforms();
    void RebuildTransformDataFromNodes(MemoryArena::memoryArena* arena);
};
