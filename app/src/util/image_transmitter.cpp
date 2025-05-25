#include "util/image_transmitter.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

#include "util/log.h"

#include <vector>


bool sc_image_transmitter_init(struct sc_image_transmitter *transmitter, 
                               const char *shm_name, 
                               size_t max_frame_size) {
    if (!transmitter || !shm_name) {
        return false;
    }
    
    memset(transmitter, 0, sizeof(*transmitter));
    
    // 复制共享内存名称
    strncpy(transmitter->shm_name, shm_name, sizeof(transmitter->shm_name) - 1);
    transmitter->shm_name[sizeof(transmitter->shm_name) - 1] = '\0';
    
    // 计算共享内存大小（头部 + 最大帧大小）
    transmitter->shm_size = sizeof(struct sc_frame_header) + max_frame_size;
    
    // 创建共享内存
    if (!sc_shared_memory_create(&transmitter->shm, shm_name, transmitter->shm_size)) {
        LOGE("Failed to create shared memory for image transmitter");
        return false;
    }
    
    // 分配 RGB 转换缓冲区
    transmitter->rgb_buffer_size = max_frame_size;
    transmitter->rgb_buffer = (uint8_t*)malloc(transmitter->rgb_buffer_size);
    if (!transmitter->rgb_buffer) {
        LOGE("Failed to allocate RGB buffer");
        sc_shared_memory_destroy(&transmitter->shm);
        return false;
    }
    
    transmitter->enabled = false;
    transmitter->frame_sequence = 0;
    
    LOGI("Image transmitter initialized: %s, size: %zu", shm_name, transmitter->shm_size);
    return true;
}

void sc_image_transmitter_destroy(struct sc_image_transmitter *transmitter) {
    if (!transmitter) {
        return;
    }
    
    if (transmitter->rgb_buffer) {
        free(transmitter->rgb_buffer);
        transmitter->rgb_buffer = NULL;
    }
    
    sc_shared_memory_destroy(&transmitter->shm);
    
    memset(transmitter, 0, sizeof(*transmitter));
    LOGI("Image transmitter destroyed");
}

bool sc_image_transmitter_send_frame(struct sc_image_transmitter *transmitter,  SDL_Renderer* renderer) {
    if (!transmitter || !renderer || !transmitter->enabled) {
        return false;
    }
    
    // 1) Get current output size of the renderer
    int width, height;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0) {
        return false;
    }
 
    assert(transmitter->rgb_buffer_size >= width * height * 4 && "rgba buffer size not enough!");

    // 3) Read the renderer's pixel data into our buffer
    //    Format: SDL_PIXELFORMAT_RGBA8888

    int read_ret = SDL_RenderReadPixels(renderer,
                            nullptr,  // entire screen
                        #ifdef _WIN32
                            SDL_PIXELFORMAT_ABGR8888,
                        #else 
                            //SDL_PIXELFORMAT_ABGR8888,
                            SDL_PIXELFORMAT_ABGR8888,
                        #endif
                            transmitter->rgb_buffer,
                            width * 4); 
    if (read_ret != 0)
    {
        LOGE("SDL_RenderReadPixels failed: %s", SDL_GetError());
        return false;
    }

    // frame head info
    struct sc_frame_header header;
    header.width = width;
    header.height = height;
    header.format = 2; // RGB24
    header.frame_size = width * height * 4;
    header.sequence = ++transmitter->frame_sequence;
    
    // time stamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    header.timestamp = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    
    header.reserved[0] = 0;
    header.reserved[1] = 0;
    
    // 写入共享内存
    if (!sc_shared_memory_write_frame(&transmitter->shm, &header, transmitter->rgb_buffer)) {
        LOGE("Failed to write frame to shared memory");
        return false;
    }
    
    // 通过 net_cmd 通知 cli-tools 有新帧可用
    char notification[128];
    snprintf(notification, sizeof(notification), 
             "{\"width\":%d,\"height\":%d,\"sequence\":%d,\"timestamp\":%llu}",
             header.width, header.height, header.sequence, header.timestamp);
    
    net_cmd_send_request("new_frame", notification);
    
    return true;
}

void sc_image_transmitter_set_enabled(struct sc_image_transmitter *transmitter, 
                                      bool enabled) {
    if (!transmitter) {
        return;
    }
    
    if (transmitter->enabled != enabled) {
        transmitter->enabled = enabled;

        //ToDo: add some implement here
    }
}

bool sc_image_transmitter_is_enabled(const struct sc_image_transmitter *transmitter) {
    return transmitter ? transmitter->enabled : false;
}
