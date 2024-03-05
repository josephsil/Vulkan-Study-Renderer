#include <cstdlib>

#include "engineGlobals.h"
#include "Code/Renderer/vulkanRenderer.h"
#include "General/InputHandling.h"

//RENDERER TODOS:
/*
 * -Submeshes
 * -Split "Render Scene" (backing data) from "Scene" (Object Data)
 * -LODs
 *      -And compacted vert buffer for binding to final draw
 * -Finer grained culling
 *      -Depth? Clusters?
 */

//GENERAL TODOS
/*
 * -Dear IMGUI
 * -Physics?
 */

void engineLoop(vulkanRenderer* renderer);


int main()
{
    printf("NEW ENTRY");
    vulkanRenderer app;
    engineLoop(&app);

    return EXIT_SUCCESS;
}

void engineLoop(vulkanRenderer* renderer)
{
    
    while(!QUIT)
    {
        InputHandler_Update();
        renderer->mainLoop(); //TODO JS: Extract non-renderer stuff out 
    }

    renderer->cleanup();
    
}