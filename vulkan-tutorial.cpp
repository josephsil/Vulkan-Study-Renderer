#include <cstdlib>

#include "engineGlobals.h"
#include "Code/Renderer/vulkanRenderer.h"
#include "General/InputHandling.h"


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