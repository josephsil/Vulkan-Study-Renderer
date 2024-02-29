#pragma once
#include <cstdint>

inline uint64_t currentFrame = 0;
inline float mouseScaleFactor = 1.0 / 1000.0f;

inline int32_t INPUT_translate_x;
inline int32_t INPUT_translate_y;
inline float INPUT_mouse_x;
inline float INPUT_mouse_y;
void InputHandler_Update();