#include "LightAndCameraHelpers.h"
#include "DebugLineData.h"

#include "Renderer/gpu-data-structs.h"


glm::mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar)
{
    float far = zNear;
    float near = zFar;
    glm::mat4 Result(1);
    Result[0][0] = (2.f) / (right - left);
    Result[1][1] = (2.f) / (top - bottom);
    Result[2][2] = (1.f) / (far - near);
    Result[3][0] = - (right + left) / (right - left);
    Result[3][1] = - (top + bottom) / (top - bottom);
    Result[3][2] = - near / (far - near);
    return Result;
}

glm::mat4 perspective_finite_z(float vertical_fov, float aspect_ratio, float n, float f)
{
    float fov_rad = vertical_fov * 2.0f * 3.141592653589f / 360.0f;
    // float focal_length = 1.0f / std::tan(fov_rad / 2.0f);
    float focal_length = 1.0f / std::tan(fov_rad / 2.0f);

    float x  =  focal_length / aspect_ratio;
    float y  = -focal_length;
    float A  = n / (f - n);
    float B  = f * A;

    glm::mat4 projection({
        x,    0.0f,  0.0f, 0.0f,
        0.0f,    y,  0.0f, 0.0f,
        0.0f, 0.0f,     A,   -1.0f,
        0.0f, 0.0f,  B, 0.0f,
    });

    return (projection);
}

//Infinite reverse Z
glm::mat4 perspective(float vertical_fov, float aspect_ratio, float n, float f)
{
    float fov_rad = vertical_fov * 2.0f * 3.141592653589f / 360.0f;
    // float focal_length = 1.0f / std::tan(fov_rad / 2.0f);
    float focal_length = 1.0f / std::tan(fov_rad / 2.0f);

    float x  =  focal_length / aspect_ratio;
    float y  = -focal_length;
    float A  = 0.f;
    float B  = n;

    glm::mat4 projection({
        x,    0.0f,  0.0f, 0.0f,
        0.0f,    y,  0.0f, 0.0f,
        0.0f, 0.0f,     A,   -1.0f,
        0.0f, 0.0f,  B, 0.0f,
    });


    return (projection);
}

viewProj LightAndCameraHelpers::CalcViewProjFromCamera(cameraData camera)
{
    Transform cameraTform = GetCameraTransform(camera);
    glm::mat4 view = cameraTform.rot * cameraTform.translation;

    glm::mat4 projInv = {};
    glm::mat4 _proj =perspective(camera.fov,
                                      camera.extent.width / static_cast<float>(camera.extent.height),
                                     
                                 camera.nearPlane,     camera.farPlane); //Inverse Z
    
    glm::mat4 proj = glm::perspective(glm::radians(camera.fov),
                                    camera.extent.width / static_cast<float>(camera.extent.height),
                                     
                                     camera.nearPlane, camera.farPlane); //Inverse Z 
    // _proj[1][1] *= -1;

    return {view, _proj};
}

Transform LightAndCameraHelpers::GetCameraTransform(cameraData camera)
{
    
    Transform output{};
    output.translation = glm::translate(glm::mat4(1.0), -camera.eyePos);
    output.rot = glm::mat4_cast(glm::conjugate(OrientationFromYawPitch(camera.eyeRotation)));

    return output;
}

glm::vec4 LightAndCameraHelpers::MaxComponentsFromSpan(std::span<glm::vec4> input)
{

    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();
    float maxW = std::numeric_limits<float>::lowest();
    for (int i = 0; i < 8; i++)
    {
        auto vec = input[i];
        maxX = std::max<float>(maxX, vec.x);
        maxY = std::max<float>(maxY, vec.y);
        maxZ = std::max<float>(maxZ, vec.z);
        maxW = std::max<float>(maxW, vec.a);
    }

    return glm::vec4(maxX,maxY,maxZ,maxW);
    
}

glm::vec4 LightAndCameraHelpers::MinComponentsFromSpan(std::span<glm::vec4> input)
{

    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float minW = std::numeric_limits<float>::infinity();
    for (int i = 0; i < 8; i++)
    {
        auto vec = input[i];
        minX = std::min<float>(minX, vec.x);
        minY = std::min<float>(minY, vec.y);
        minZ = std::min<float>(minZ, vec.z);
        minW = std::min<float>(minW, vec.a);
    }

    return glm::vec4(minX,minY,minZ,minW);
    
}

std::span<glm::vec4> LightAndCameraHelpers::FillFrustumCornersForSpace(std::span<glm::vec4> output_span,
                                                                       glm::mat4 matrix)
{
    const glm::vec3 frustumCorners[8] = {
        glm::vec3( 1.0f, -1.0f, 1.0f), // bot right (far) 
        glm::vec3(-1.0f, -1.0f, 1.0f), //bot left (far) 
        glm::vec3(-1.0f,  1.0f, 1.0f), //top rgiht  (far) 
        glm::vec3( 1.0f,  1.0f, 1.0f), // top left (far) 
        glm::vec3( 1.0f, -1.0f,  0.0f + 0.0001f), //bot r8ghyt  (near)-- offset due to infinite far plane
        glm::vec3(-1.0f, -1.0f,  0.0f + 0.0001f), //bot left (near)-- offset due to infinite far plane
        glm::vec3(-1.0f,  1.0f,  0.0f + 0.0001f), //top left (near)-- offset due to infinite far plane
        glm::vec3( 1.0f,  1.0f,  0.0f + 0.0001f), //top right (near)-- offset due to infinite far plane
    };

    assert (output_span.size() == 8);
    for(int j = 0; j < 8; j++)
    {
        glm::vec4 invCorner =  matrix * glm::vec4(frustumCorners[j], 1.0f);
        output_span[j] =  invCorner /  (invCorner.w ) ;
    }

    return output_span;

}


//Light stuff. I kinda struggled through the math here initially -- needs a rewrite.
std::span<GPU_perShadowData> LightAndCameraHelpers::CalculateLightMatrix(MemoryArena::Allocator* allocator,
                                                                     cameraData cam, glm::vec3 lightPos, glm::vec3 spotDir, float spotRadius, LightType type,
                                                                     DebugLineData* debugLinesManager)
{
    viewProj vp = CalcViewProjFromCamera(cam);
    
    glm::vec3 dir = type ==  LIGHT_SPOT ? spotDir : glm::normalize(lightPos);
    glm::vec3 up = glm::vec3( 0.0f, 1.0f,  0.0f);
    //offset directions that are invalid for lookat
    if (abs(up) == abs(dir))
    {
        dir += glm::vec3(0.00001F);
        dir = glm::normalize(dir);
    }
    glm::mat4 lightViewMatrix = {};
    glm::mat4 lightProjection = {};
    debugLinesManager->addDebugCross(lightPos, 2, {1,1,0});
    std::span<GPU_perShadowData> outputSpan;
    switch (type)
    {
      
    case LIGHT_DIR:
        {

            outputSpan = MemoryArena::AllocSpan<GPU_perShadowData>(allocator, CASCADE_CT ); 

            glm::mat4 invCam = glm::inverse(vp.proj * vp.view);

            glm::vec4 frustumCornersWorldSpace[8] = {};
            FillFrustumCornersForSpace(frustumCornersWorldSpace, invCam);

            debugLinesManager->AddDebugFrustum(frustumCornersWorldSpace);

            float min_cascade = cam.nearPlane + 0.1f;
            float maxZ =  (50.f ) - min_cascade;
            float minZ =  min_cascade;
            float clipRange = (maxZ - min_cascade);

            float range = maxZ - minZ;
            float ratio = maxZ / minZ;

            //cascades -- arbitrary numbers for this scene
            float cascadeSplits[CASCADE_CT] = {
				1.0f - 0.965f,
			1.0f - 0.945f,
			1.0f - 0.92f,
			1.0f - 0.89f,
			1.0f - 0.78f,
			1.0f - 0.42f};
           

            for (int i = 0; i < CASCADE_CT; i++)
            {
                glm::vec3 debugPos = glm::vec3(cam.eyePos);
                float splitDist = cascadeSplits[i];
                float lastSplitDist = i == 0 ? 0 : cascadeSplits[i-1]; 
                
                glm::vec3 frustumCornersWorldSpacev3[8] = {}; //TODO JS: is this named correctly?
                for(int j = 0; j < 8; j++)
                {
                    frustumCornersWorldSpacev3[j] = glm::vec3(frustumCornersWorldSpace[j]);
                }
                for(int j = 0; j < 4; j++)
                { //Dont think this is right, think my frustum is oriented differently
                    glm::vec3 dist = (frustumCornersWorldSpacev3[j + 4] - frustumCornersWorldSpacev3[j] );
                    frustumCornersWorldSpacev3[j + 4] = frustumCornersWorldSpacev3[j] + (dist * splitDist);
                    frustumCornersWorldSpacev3[j] = frustumCornersWorldSpacev3[j] + (dist * lastSplitDist);
                }

                glm::vec3 frustumCenter = glm::vec3(0.0f);
                for (uint32_t j = 0; j < 8; j++) {
                    frustumCenter += frustumCornersWorldSpacev3[j];
                }
                frustumCenter /= 8.0f;

                float radius = 0.0f;
                for (uint32_t i = 0; i < 8; i++) {
                    float distance = glm::length(glm::vec3(frustumCornersWorldSpacev3[i]) - frustumCenter);
                    radius = glm::max<float>(radius, distance);
                }
                radius = std::ceil(radius * 16.0f) / 16.0f;


                debugLinesManager->addDebugCross(debugPos, 2, {0,static_cast<float>(i) /  static_cast<float>(CASCADE_CT),0});
               
                float distanceScale =2.5f;


                //Texel clamping
                glm::mat4 texelScalar = glm::mat4(1);
                texelScalar = glm::scale(texelScalar, glm::vec3((float)SHADOW_MAP_SIZE / (radius * 2.0f)));
                glm::vec3 maxExtents = glm::vec3(radius);
                glm::vec3 minExtents = -maxExtents;
                glm::mat4 baseLightvew = glm::lookAt(normalize(dir), glm::vec3(0),  up);
                glm::mat4 texelLightview = texelScalar * baseLightvew ;
                glm::mat4 texelInverse = (inverse(texelLightview));
                glm::vec4 transformedCenter = texelLightview * glm::vec4(frustumCenter, 1.0);
                transformedCenter.x = floor(transformedCenter.x);
                transformedCenter.y = floor(transformedCenter.y);
                transformedCenter = texelInverse * transformedCenter ;
                //Compute output matrices
                //TODO: Derive the correc tortho matrices -- these produce weird opengl style depth buffers, with everything clustered around 0.5
                float farPlane = (min_cascade + (maxExtents.z - minExtents.z) * distanceScale);
                lightViewMatrix = glm::lookAt(glm::vec3(transformedCenter)  + ((dir * maxExtents) * distanceScale), glm::vec3(transformedCenter), up);
                glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, farPlane, 0.001f) ;

                lightProjection =  lightOrthoMatrix;
                outputSpan[i] = {lightViewMatrix, lightProjection,  farPlane, 0.001f/*todo*/,(min_cascade + ((maxExtents.z - minExtents.z) /3.0f)  )}; // /3 here is a hacky number -- need to fix all this
            }
            return  outputSpan.subspan(0,CASCADE_CT);
        }
    case LIGHT_SPOT:
        {
            outputSpan = MemoryArena::AllocSpan<GPU_perShadowData>(allocator, 1 ); 
            lightViewMatrix = glm::lookAt(lightPos, (lightPos - dir), up);
            glm::mat4 
            lightProjection = perspective(spotRadius *2.f,
                                               1.0f,  0.001f, 2500.f
                                               ); //TODO BETTER FAR 
            outputSpan[0] = {lightViewMatrix, lightProjection, 0, 0.001f, 0};
                                  
            return  outputSpan;
        }
    case LIGHT_POINT:

        {
            outputSpan = MemoryArena::AllocSpan<GPU_perShadowData>(allocator, 6 ); 
            lightProjection = perspective_finite_z(90,
                                               1.0f, POINT_LIGHT_NEAR_PLANE,
                                               POINT_LIGHT_FAR_PLANE);} 
        lightProjection[1][1] *= -1; //TODO JS hack -- these are rendering upside down for some reason, fix should probably be elsewhere
        for(int i = 0; i < outputSpan.size(); i++)
        {
            outputSpan[i].nearPlane = POINT_LIGHT_NEAR_PLANE;
            outputSpan[1].farPlane = 0;
            outputSpan[i].depth = 0;
        }
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), -lightPos);
        glm::mat4 rotMatrix = glm::mat4(1.0);
    
        outputSpan[0].viewMatrix =  glm::rotate(rotMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        outputSpan[0].viewMatrix =  (glm::rotate(  outputSpan[0].viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
        rotMatrix = glm::mat4(1.0);
        outputSpan[1].viewMatrix = glm::rotate(rotMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        outputSpan[1].viewMatrix =  (glm::rotate(outputSpan[1].viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
        rotMatrix = glm::mat4(1.0);
        outputSpan[2].viewMatrix = (glm::rotate(rotMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
        rotMatrix = glm::mat4(1.0);
        outputSpan[3].viewMatrix = (glm::rotate(rotMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
        rotMatrix = glm::mat4(1.0);
        outputSpan[4].viewMatrix = (glm::rotate(rotMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * translation);
        rotMatrix = glm::mat4(1.0);
        outputSpan[5].viewMatrix = (glm::rotate(rotMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * translation);
        rotMatrix = glm::mat4(1.0);

        for(int i =0; i < 6; i++)
        {
            outputSpan[i].projMatrix = lightProjection;
          
        }
        return  outputSpan;
    }

    assert(!outputSpan.empty());
    return outputSpan;

}
