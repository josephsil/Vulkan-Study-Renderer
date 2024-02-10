#pragma once
#include <memory>
#include <memory>
#include <queue>
#include <span>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include <General/Array.h>
#include <Renderer/rendererGlobals.h>
//
struct positionRadius;

namespace MemoryArena
{
    struct memoryArena;
}

//Objects have like, transformation info, ref to their mesh, ref to their material
//Not sure on ref to material. Really I only have one shader right now
struct TextureData;
struct MeshData;
struct VkDescriptorImageInfo;
class Scene;


//TODO JS: Transform hiearchy 
//Basic idea: loop over each level of the hiearchy and compute all the matrices
//In theory each level could be done in parallel
//so like
// [0][0][0][0][0][0][0][0][1][1][1][2][2][2][2][3][3][4]
//Within each sub-level I'll order them by common parent, like
// [1a][1a][1b][2a][2b][2c][2c][3a][3a][4a]
// That ought to give us cache hits looking to parent, right, which will help in the case of big flat levels
//All the 0s have no dependencies -- all the 1s depend on one of the 0s, and so on
//How to store?
//In theory one backing array, but I may do an array of arrays per level for add/remove convenience
//Then a parallel(?) array of indices back to parent, which is walkable to get up graph
//Finally I'll need to capture the span of each level somewhere for dispatching work for them
//Either a short array of indices or just spans.

//And then for update: the game will always submit an update for the NEXT frame
//We'll always compute all updates and run whole graph at the start of each frame.
//So we always query up to date info, and we always queue up info for next frame.

//Would need two paralell sets of matrices -- world and local. For the operations above, we loop over LOCAL, and read/write WORLD
//Local doesn't change, world does. We look at our parents' world.
//For the loop, we skip over level 0, since the root doesn't have a local transform.

//What do we do to 'insert' new nodes to a level?
//Most common for root level 0.
//Don't order 0, not an issue.  
//Everything on the next level could need its parent node updated
//Everything on the next level AFTER the insertion needs its parent node updated

/*
 *if (parent.childcount != 0) //this is an insert
 *insertionIDX = 0
 *for(int i = 0; i < level.size(); i++
 *if (node[i].parent == parent)
 *  insertionIDX = i
 *  break;
 *subspan = level.subspan(insertionIDX, level.size)
 *for(int i =subspan.size -1; i > 0; i--)
 *  node[i+1] = node[i]
 *node[inseritionIDX] = newNode;
 * //NEXT, fix up children 
 *nextLevel = level + 1;
 *insertion = insertionIDX
 *for(int i = 0; i < nextLevel.size(); i++)
 *  if (node[i].parent > insertionIDX)
 *      node.parent +=1
 * ?????
 * else just add it to the end.
 * Honestly not sure sorting the levels is worth it. 
 *
 //May just be better to be naieve and rebuild the whole hiearchy, then I can use the same logic for unparent and change parent.
 //How does unparenting work?
 //Just mark the spot empty and skip?
 //Maybe I can do something simpler, and just bump anyone who gets updated to the back
 //Pop all of them off, compact 
 */
/* [1a][1a][1b][2a][2b][2c][2c][3a][3a][4a] ->
 * [1a][1a][++1a++][1b][2a][2b][2c][2c][3a][3a][4a]->
 * [1b][1a][1a][1a][2a][2b][2c][2c][3a][3a][4a]
 * How? Loop over everyone after the start of the parentSet and subtract the size of the parentSet from their index
 * Then add the parentSet to the end. This would do subtraction too, bump the subtraction to the end with one clipped off.
 * Then for the next depth, apply all the same moves to fix up parent pointers
 * #### I think I like this. ####
 * How to handle world matrices? 
 * They get re-written every frame
 * I should move it to the START, that way I don't have to update as many
 * Only the ones before it get bumped 
 * 
*/



//Tree representation for inserting 
struct localTransform
{
    glm::mat4 matrix;
    std::string name;
    uint8_t depth;
    
    std::vector<std::shared_ptr<localTransform>> children; 
};

std::shared_ptr<localTransform> AddChild(localTransform* tgt, std::string childName, glm::mat4 childMat);

void rmChild(localTransform* tgt, std::shared_ptr<localTransform> remove);


//Flat representation for updating
struct flatlocalTransform
{
    glm::mat4 matrix;
    std::string name;
    uint8_t parent;
};

inline std::vector<std::vector<flatlocalTransform>> localTransformHiearchy; // [depth][object]

void flattenTransformHiearchy(std::span<localTransform> roots);
void InitializeScene(MemoryArena::memoryArena* arena, Scene* scene);
class Scene
{
    //Should add a like gameobject-y thing that has a mesh and a material
private:
    //No scale for now
public:
    int utilityTextureCount();
    int materialCount();
    int materialTextureCount();

    void OrderedObjectIndices(::MemoryArena::memoryArena* allocator, glm::vec3 eyePos, std::span<int> indices, bool invert);
    struct Objects
    {
        int objectsCount = 0;
        //Parallel arrays per-object
        Array<glm::vec3> translations;
        Array<glm::quat> rotations;
        Array<glm::vec3> scales;
        Array<uint32_t> meshIndices;
        Array<Material> materials;
        Array<glm::mat4> matrices;
    };

    Objects objects;

    // arallel arrays per Light
    int lightCount = 0;


    Array<uint32_t> lightshadowMapCount;
    Array<glm::vec4> lightposandradius;
    Array<glm::vec4> lightcolorAndIntensity;
    Array<glm::vec4> lightDir;
    Array<glm::float32> lightTypes;
    

    //Non parallel arrays //TODO JS: Pack together?
    int textureSetCount = 0;
    Array<TextureData> backing_diffuse_textures;
    Array<TextureData> backing_specular_textures;
    Array<TextureData> backing_normal_textures;

    int _utilityTextureCount = 0;
    Array<TextureData> backing_utility_textures;

    int meshCount = 0;
    Array<MeshData> backing_meshes;
    Array<positionRadius> meshBoundingSphereRad;


    Scene(MemoryArena::memoryArena* memoryArena);
    void Update();
    //Returns the index to the object in the vectors
    int AddObject(MeshData* mesh, int textureidx, float material_roughness, bool material_metallic, glm::vec3 position,
                  glm::quat rotation, glm::vec3 scale = glm::vec3(1));
    uint32_t getVertexCount();
    int objectsCount();
    uint32_t getOffsetFromMeshID(int id);

    //TODO JS: these are temporary
    int AddUtilityTexture(TextureData T);
    int AddMaterial(TextureData D, TextureData S, TextureData N);
    int AddBackingMesh(MeshData M);
    positionRadius GetBoundingSphere(int idx);

    int AddDirLight(glm::vec3 position, glm::vec3 color,float intensity);
    int AddSpotLight(glm::vec3 position, glm::vec3 dir, glm::vec3 color, float radius, float intensity);
    int AddPointLight(glm::vec3 position, glm::vec3 color,  float intensity);
    int getShadowDataIndex(int i);
    int shadowCasterCount();
    uint32_t shadowmapCount;

    void Cleanup();

    std::pair<std::vector<VkDescriptorImageInfo>, std::vector<VkDescriptorImageInfo>> getBindlessTextureInfos(MemoryArena::memoryArena* allocator);
private:
    int AddLight(glm::vec3 position, glm::vec3 dir = glm::vec3(-1), glm::vec3 color = glm::vec3(1), float radius = 1, float intensity = 1, lightType type = lightType::LIGHT_POINT);
    void lightSort();
};
