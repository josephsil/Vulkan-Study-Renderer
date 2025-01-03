#include "InputHandling.h"
#include <cstdio>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_timer.h>
#include "engineGlobals.h"
#include "backends/imgui_impl_sdl2.h"
#include "Renderer/RendererInterface.h"

uint32_t T2;
uint32_t T;

//timeouts for keys -- todo replace/rename with functionality
uint32_t k_timeout;
uint32_t l_timeout;
uint32_t arrow_timeout = 0;
void InputHandler_Update(bool disableMouse)
{
        SDL_Event e;
        bool bQuit = false;

        T2 = SDL_GetTicks();
        uint32_t deltaTicks = T2 - T;
        deltaTime = deltaTicks * 0.001f;
        T = SDL_GetTicks();
    
        INPUT_mouse_x = 0;
        INPUT_mouse_y = 0;


        //Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            //close the window when user clicks the X button or alt-f4s
            if (e.type == SDL_QUIT) bQuit = true;
            if (bQuit) QUIT = true;


            SDL_PumpEvents();
            const uint8_t* KeyboardState = SDL_GetKeyboardState(nullptr);
            INPUT_translate_x = 0 - KeyboardState[SDL_SCANCODE_A] + KeyboardState[SDL_SCANCODE_D];
            INPUT_translate_y = 0 + KeyboardState[SDL_SCANCODE_W] - KeyboardState[SDL_SCANCODE_S];
            int x, y;

            if (!disableMouse)
            {
                const uint32_t MouseButtonState = SDL_GetRelativeMouseState(&x, &y);


                if (MouseButtonState & SDL_BUTTON_LMASK) // mouse down
                    {
                    float fx = x * mouseScaleFactor;
                    float fy = y * mouseScaleFactor;
                    INPUT_mouse_x = fx;
                    INPUT_mouse_y = fy;
                    }
            }



            //RENDER DEBUGGING STUFF
            //Probably doesn't need to get directly set from here but whatever works
            if (KeyboardState[SDL_SCANCODE_K])
            {
                if (T > k_timeout)
                {
                    debug_cull_freeze = !debug_cull_freeze;
                    if (debug_cull_freeze) printf("froze debug culling \n");
                    else printf("unfroze debug culling \n");
                    k_timeout = T + 200;
                }
            }

            if (KeyboardState[SDL_SCANCODE_L])
            {
                if (T > l_timeout)
                {
                    debug_cull_override = !debug_cull_override;
                    if (debug_cull_override)printf("enabled debug culling index: %d! \n", debug_cull_override_index);
                    else printf("debug culling disabled \n");
                    l_timeout = T + 200;
                }
            }
            if (KeyboardState[SDL_SCANCODE_DOWN] || KeyboardState[SDL_SCANCODE_UP])
            {
                if (debug_cull_override)
                {
                    if (T > arrow_timeout)
                    {
                        debug_cull_override_index = (debug_cull_override_index - (int)KeyboardState[SDL_SCANCODE_DOWN] +
                            (int)KeyboardState[SDL_SCANCODE_UP]);
                        arrow_timeout = T + 100;
                    }
                }
            }
            if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_z)
                {
                    debug_shader_bool_1 = debug_shader_bool_1 ? 0 : 1; //debug toggle float we can bool off of on shader
                }
            }
        }

        
}
