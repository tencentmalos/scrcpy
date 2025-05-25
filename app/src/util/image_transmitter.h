#ifndef SC_IMAGE_TRANSMITTER_H
#define SC_IMAGE_TRANSMITTER_H

#include <stdbool.h>
#include <stdint.h>
#include <libavutil/frame.h>
#include <SDL2/SDL.h>

#include "util/file_mapping.h"
#include "net_cmd/net_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif



struct sc_image_transmitter {
    struct sc_file_mapping mapping;
    bool enabled;
    uint32_t frame_sequence;
    char shm_name[64];
    size_t shm_size;
    
    uint8_t *rgb_buffer;
    size_t rgb_buffer_size;
};


bool sc_image_transmitter_init(struct sc_image_transmitter *transmitter, 
                               const char *shm_name, 
                               size_t max_frame_size);


void sc_image_transmitter_destroy(struct sc_image_transmitter *transmitter);


bool sc_image_transmitter_send_frame(struct sc_image_transmitter *transmitter, 
                                     SDL_Renderer* renderer);


void sc_image_transmitter_set_enabled(struct sc_image_transmitter *transmitter, 
                                      bool enabled);


bool sc_image_transmitter_is_enabled(const struct sc_image_transmitter *transmitter);


#ifdef __cplusplus
}
#endif

#endif // SC_IMAGE_TRANSMITTER_H
