#ifndef SC_DISPLAY_H
#define SC_DISPLAY_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavutil/frame.h>
#include <SDL2/SDL.h>

#include "coords.h"
#include "opengl.h"
#include "options.h"

#include "util/image_transmitter.h"

#ifdef __APPLE__
# define SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
#endif

struct sc_display {
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    struct sc_image_transmitter* image_transmitter;

    struct sc_opengl gl;
#ifdef SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
    SDL_GLContext *gl_context;
#endif

    bool mipmaps;

    struct {
#define SC_DISPLAY_PENDING_FLAG_SIZE 1
#define SC_DISPLAY_PENDING_FLAG_FRAME 2
        int8_t flags;
        struct sc_size size;
        AVFrame *frame;
    } pending;

    bool has_frame;

    uint64_t last_updated_frame_count;
    uint64_t copy_shared_frame_index;
};

enum sc_display_result {
    SC_DISPLAY_RESULT_OK,
    SC_DISPLAY_RESULT_PENDING,
    SC_DISPLAY_RESULT_ERROR,
};

enum sc_eye_mode {
    SC_EYE_MODE_TWOEYES = 0,
    SC_EYE_MODE_LEFT = 1, 
    SC_EYE_MODE_RIGHT = 2,
};

bool
sc_display_init(struct sc_display *display, SDL_Window *window,
                SDL_Surface *icon_novideo, bool mipmaps, 
                struct sc_image_transmitter* image_transmitter);

void
sc_display_destroy(struct sc_display *display);

enum sc_display_result
sc_display_set_texture_size(struct sc_display *display, struct sc_size size);

enum sc_display_result
sc_display_update_texture(struct sc_display *display, const AVFrame *frame);

enum sc_display_result
sc_display_render(struct sc_display *display, const SDL_Rect *geometry,
                  enum sc_orientation orientation, struct sc_size content_size, enum sc_eye_mode eye_mode);

#endif
