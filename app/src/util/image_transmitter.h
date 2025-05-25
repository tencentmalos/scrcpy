#ifndef SC_IMAGE_TRANSMITTER_H
#define SC_IMAGE_TRANSMITTER_H

#include <stdbool.h>
#include <stdint.h>
#include <libavutil/frame.h>
#include <SDL2/SDL.h>

#include "util/shared_memory.h"
#include "net_cmd/net_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif


// 图像传输器结构
struct sc_image_transmitter {
    struct sc_shared_memory shm;
    bool enabled;
    uint32_t frame_sequence;
    char shm_name[64];
    size_t shm_size;
    
    // 图像格式转换缓冲区
    uint8_t *rgb_buffer;
    size_t rgb_buffer_size;
};

// 初始化图像传输器
bool sc_image_transmitter_init(struct sc_image_transmitter *transmitter, 
                               const char *shm_name, 
                               size_t max_frame_size);

// 销毁图像传输器
void sc_image_transmitter_destroy(struct sc_image_transmitter *transmitter);

// 传输帧数据
bool sc_image_transmitter_send_frame(struct sc_image_transmitter *transmitter, 
                                     SDL_Renderer* renderer);

// 启用/禁用传输
void sc_image_transmitter_set_enabled(struct sc_image_transmitter *transmitter, 
                                      bool enabled);

// 检查是否启用
bool sc_image_transmitter_is_enabled(const struct sc_image_transmitter *transmitter);


#ifdef __cplusplus
}
#endif

#endif // SC_IMAGE_TRANSMITTER_H
