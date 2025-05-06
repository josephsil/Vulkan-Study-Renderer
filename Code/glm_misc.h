#pragma once
#define GLM_FORCE_RADIANS	
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_GTC_quaternion
#include <glm/gtx/quaternion.hpp>

glm::quat OrientationFromYawPitch(glm::vec2 yawPitch);
