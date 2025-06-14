#include "glm_misc.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_GTC_quaternion
#include <glm/gtx/quaternion.hpp>

glm::quat OrientationFromYawPitch(glm::vec2 yawPitch)
{
    glm::quat yawQuat = angleAxis(glm::radians(yawPitch.x), glm::vec3(0, -1, 0));
    glm::quat pitchQUat = angleAxis((glm::radians(-yawPitch.y)), glm::vec3(1, 0, 0));
    return yawQuat * pitchQUat;
}
