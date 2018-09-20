#include <stdio.h>
#include <iostream>
#include "EngineApplication.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

// Memory leak detection
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

int main() {
    // Run the app
    try {
        EngineApplication app = EngineApplication();
        app.Run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        //_CrtDumpMemoryLeaks();
        //return EXIT_FAILURE;
    }
    _CrtDumpMemoryLeaks();

    return EXIT_SUCCESS;
}
