#include "capture_screen.h"

extern "C" {
#include "log.h"
}

#include <vector>

// Include stb_image_write for saving PNG
#define STB_IMAGE_WRITE_IMPLEMENTATION  // Generate the implementation in this translation unit
#include "stb/stb_image_write.h"

static bool save_screen_shot_impl(const char* filename, SDL_Renderer* renderer) {
    // 1) Get current output size of the renderer
    int width, height;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0) {
        LOGE("SDL_GetRendererOutputSize failed: %s", SDL_GetError());
        return false;
    }

    // 2) Allocate buffer for RGBA pixels
    std::vector<Uint8> pixels(width * height * 4);

    // 3) Read the renderer's pixel data into our buffer
    //    Format: SDL_PIXELFORMAT_RGBA8888

    int read_ret =SDL_RenderReadPixels(renderer,
                             nullptr,  // entire screen
                        #ifdef _WIN32
                             SDL_PIXELFORMAT_ABGR8888,
                        #else 
                             //SDL_PIXELFORMAT_ABGR8888,
                             SDL_PIXELFORMAT_ABGR8888,
                        #endif
                             pixels.data(),
                             width * 4); 
    if (read_ret != 0)
    {
        LOGE("SDL_RenderReadPixels failed: %s", SDL_GetError());
        return false;
    }

    // 4) Use stbi_write_png to save the image
    //    - Channels = 4 (RGBA)
    //    - stride   = width * 4
    if (stbi_write_png(filename, width, height, /*channels=*/4, pixels.data(), width * 4) == 0) {
        LOGE("stbi_write_png failed to write file: %s", filename);
        return false;
    }

    LOGI("Screenshot saved as %s", filename);
    return true;
}




extern "C" {

int sc_save_screen_shot(const char* filename, SDL_Renderer* renderer) {
    if(save_screen_shot_impl(filename, renderer)) {
        return 0;
    }

    return -1;
}

}


