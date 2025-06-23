#ifndef OBJECTDATAMACROS
#define OBJECTDATAMACROS

#define VERTEXOFFSET PerObjectData[InstanceIndex].props.indexInfo.g
#define TEXTURESAMPLERINDEX  PerObjectData[InstanceIndex].props.textureIndexInfo.g
#define NORMALSAMPLERINDEX  PerObjectData[InstanceIndex].props.textureIndexInfo.b 
#define SHADERINDEX  PerObjectData[InstanceIndex].props.textureIndexInfo.a
#define TRANSFORMINDEX  PerObjectData[InstanceIndex].props.indexInfo.a
#define GetTransform()  Transforms[PerObjectData[InstanceIndex].props.indexInfo.a]
#define GetModelInfo() PerObjectData[InstanceIndex].props
#define MODEL_MATRIX (GetTransform().Model)
#define MODEL_VIEW_MATRIX mul(VIEW_MATRIX, MODEL_MATRIX)
#define MVP_MATRIX mul(PROJ_MATRIX, MODEL_VIEW_MATRIX)

#endif // #OBJECTDATAMACROS