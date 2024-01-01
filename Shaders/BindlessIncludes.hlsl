#define USE_RW

#define LIGHT_DIR 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define LIGHTCOUNT   globals.lightcount_mode_shadowct_padding.r
#define VERTEXOFFSET pc.indexInfo.g
#define TEXTURESAMPLERINDEX pc.indexInfo.b
#define NORMALSAMPLERINDEX TEXTURESAMPLERINDEX +2 //TODO JS: temporary!
#define OBJECTINDEX  pc.indexInfo.a
#define LIGHTINDEX   pc.indexInfo_2.a
#define SKYBOXLUTINDEX globals.lutIDX_lutSamplerIDX_padding_padding.x
#define SKYBOXLUTSAMPLERINDEX globals.lutIDX_lutSamplerIDX_padding_padding.y
#define SHADOWCOUNT globals.lightcount_mode_shadowct_padding.z

#ifdef SHADOWPASS
#define MATRIXOFFSET pc.indexInfo_2.b
#endif


struct UBO
{
    float4x4 Model;
    float4x4 NormalMat;
    float4x4 p1;
    float4x4 p2;
};

struct ShaderGlobals
{
    float4x4 view;
    float4x4 projection;
    float4 viewPos;
    float4 lightcount_mode_shadowct_padding;
    float4 lutIDX_lutSamplerIDX_padding_padding;
};

struct pconstant
{
    float4 indexInfo;
    float roughness;
    float metallic;
    float _f1;
    float _f2;
    float4 indexInfo_2;
    float4 _2;
};

struct MyVertexStructure
{
    float4 position;
    float4 uv0;
    float4 normal;
    float4 Tangent;
    // uint color;
};

struct MyLightStructure
{
    float4 position_range;
    float4 color_intensity;
    float4 lighttype_lightDir;
    float4x4 matrixViewProjeciton;
    float4 matrixIDX_matrixCt_padding; // currently only used by point
    // uint color;
};

cbuffer globals : register(b0) { ShaderGlobals globals; }
// ShaderGlobals globals;
// uniformDescriptorSets
// storageDescriptorSets
// imageDescriptorSets
// samplerDescriptorSets

[[vk::binding(0, 2)]]
Texture2D<float4> bindless_textures[];

[[vk::binding(0, 3)]]
SamplerState bindless_samplers[];

[[vk::binding(2, 2)]]
Texture2D<float4> shadowmap[];
[[vk::binding(2, 2)]]
TextureCube shadowmapCube[];

[[vk::binding(2, 3)]]
SamplerComparisonState shadowmapSampler[];

[[vk::binding(0,1)]]
#ifdef USE_RW
RWStructuredBuffer<MyVertexStructure> BufferTable;
#else
ByteAddressBuffer BufferTable;
#endif
[[vk::binding(1,1)]]
RWStructuredBuffer<MyLightStructure> lights;
[[vk::binding(2, 1)]]
RWStructuredBuffer<UBO> uboarr;
#ifdef SHADOWPASS
[[vk::binding(3, 1)]]
RWStructuredBuffer<float4x4> shadowMatrices;
#endif 


[[vk::binding(1,2)]]
TextureCube cubes[];

[[vk::binding(1,3)]]
SamplerState cubeSamplers[];

[[vk::push_constant]]
pconstant pc;

#define  DIFFUSE_INDEX  (TEXTURESAMPLERINDEX * 3) + 0
#define  SPECULAR_INDEX  (TEXTURESAMPLERINDEX * 3) + 1
#define  NORMAL_INDEX  (TEXTURESAMPLERINDEX * 3) + 2

float3 GET_SPOT_LIGHT_DIR(MyLightStructure light)
{
    return light.lighttype_lightDir.yzw;
}

int getLightType(MyLightStructure light)
{
    return light.lighttype_lightDir.x;
}

int getShadowMatrixIndex(MyLightStructure light)
{
    return light.matrixIDX_matrixCt_padding.r;
}


float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(
        clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float PI = 3.14159265359;

    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float2 pointShadowDirection(int idx)
{
    return float2((idx < 3 ? 0 : 0.5), (0.333 * (idx % 3)));
}

float uvToPointShadow(int idx, float2 uv)
{
    float2 offset = pointShadowDirection(idx);
    uv *= float2(0.5,0.33);
    uv += offset;
    return uv;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// int uvIndexFromDir(float3 dir)
// {
//     
// }

float VectorToDepthValue(float3 Vec)
{
    float3 AbsVec = abs(Vec);
    float LocalZcomp = max(AbsVec.x, max(AbsVec.y, AbsVec.z));

    const float FAR = 10.0;
    const float NEAR = 0.001;
    float NormZComp = (FAR+NEAR) / (FAR-NEAR) - (2.0*FAR*NEAR)/(FAR-NEAR)/LocalZcomp;
    return (NormZComp + 1.0) * 0.5;
}


float3 getShadow(int index, float3 fragPos)
{
    MyLightStructure light = lights[index];
    if (getLightType(light) == LIGHT_POINT)
    {
      
        float3 fragToLight =  fragPos - light.position_range.xyz ;
        float3 dir = normalize(fragToLight);
        dir = dir.xyz;
        dir *= float3(1,1,1);

        float distLightSpace = VectorToDepthValue( light.position_range.xyz - fragPos);
        float dist =  distance(light.position_range.xyz, fragPos) ;
        float3 proj = dist / 10;
        float3 shadow = (shadowmapCube[index].SampleLevel(cubeSamplers[0], fragToLight, 0.0).r - 0.0) * 1;
        // shadow = pow(shadow, 700);
        
       // return proj.z;
        float output;
        return distLightSpace < (shadow.r ); // float3(proj.z, shadow.r, 1.0);
    }
    float4x4 lightMat = light.matrixViewProjeciton;
    float4 fragPosLightSpace = mul( lightMat, float4(fragPos, 1.0));
    float3 shadowProjection = (fragPosLightSpace.xyz / fragPosLightSpace.w);
    float3 shadowUV = shadowProjection   * 0.5 + 0.5;
    // shadowProjection.y *= -1;
    return shadowmap[index].SampleCmpLevelZero(shadowmapSampler[0], shadowUV.xy, shadowProjection.z).r; //TODO JS: dont sample on branch?

}

float3 getLighting(float4x4 model, float3 albedo, float3 inNormal, float3 FragPos, float3 F0, float3 roughness, float metallic)
{
    float PI = 3.14159265359;
    float3 viewDir = normalize(globals.viewPos - FragPos);
    float3 lightContribution = float3(0, 0, 0);
    // albedo = 1.0;
    // roughness = 1.0;
// metallic = 0.0;
    for (int i = 0; i < LIGHTCOUNT; i++)
    {
        MyLightStructure light = lights[i];
        float3 lightPos = light.position_range.xyz;
        float3 lightColor = light.color_intensity.xyz * light.color_intensity.a;
        float3 lightDir;
        float3 lightToFrag = lightPos - (FragPos); // frag pos * 0 if it's a directional light

        if (getLightType(light) == LIGHT_POINT || getLightType(light) == LIGHT_SPOT)
        {
            lightDir = normalize(lightToFrag);
        }
        else if (getLightType(light) == LIGHT_DIR)
        {
            lightDir = lightPos;
        }
        
        
        float lightDistance = pow(length(lightPos - FragPos), 2);

        float3 halfwayDir = normalize(lightDir + viewDir);

        float3 radiance = lightColor;
        int lightType = getLightType(light);
        if (lightType == LIGHT_POINT || lightType == LIGHT_SPOT)
        {
            float attenuation = 1.0 / (lightDistance * lightDistance);
            radiance *= attenuation;
        }
        if (lightType == LIGHT_SPOT)
        {
            float3 spotlightDir = GET_SPOT_LIGHT_DIR(light);
            float theta = dot(normalize(lightDir), normalize(-spotlightDir));

            float cosCutoff = (1.0 - (light.position_range.a) / 180);
            float softFactor = 1.2; //TODO JS: paramaterize 
            float cosCutoffOuter = (1.0 - (light.position_range.a * softFactor) / 180);
            float epsilon   = cosCutoff - cosCutoffOuter ;
            
                radiance *= clamp((theta - cosCutoffOuter) / epsilon, 0.0, 1.0);    
        }
        
        float3 Fresnel = fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0);
        float NDF = DistributionGGX(inNormal, halfwayDir, roughness);
        float G = GeometrySmith(inNormal, viewDir, lightDir, roughness);
        float3 numerator = NDF * G * Fresnel;
        float denominator = 4.0 * max(dot(inNormal, viewDir), 0.0) * max(dot(inNormal, lightDir), 0.0) + 0.0001;
        float3 specular = numerator / denominator;
        float3 kS = Fresnel;
        float3 kD = 3.0 - kS;

        kD *= 1.0 - metallic;
        float NdotL = max(dot(inNormal, lightDir), 0.0);
       float3 lightAdd = (kD * albedo / PI + specular) * radiance * NdotL;

        //Shadow:

        if (i < SHADOWCOUNT) //TODO JS: pass in max shadow casters?
        {
         
         
            float shadowMapValue = getShadow(i, FragPos);

            // if (i == 0)
            // {
            //     return  getShadow(i, FragPos);;
            // }
            // float shadow = shadowProjection.z < (shadowMapValue) ? 1.0 : 0.0;
            //TODO: vias by normal
            //TODO: cascade
           lightAdd *= shadowMapValue;
        }
        //

        lightContribution +=  lightAdd;
    }


    return lightContribution;
}