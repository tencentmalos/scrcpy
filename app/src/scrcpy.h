#ifndef SCRCPY_H
#define SCRCPY_H

#include "common.h"

#include "options.h"

enum scrcpy_exit_code {
    // Normal program termination
    SCRCPY_EXIT_SUCCESS,

    // No connection could be established
    SCRCPY_EXIT_FAILURE,

    // Device was disconnected while running
    SCRCPY_EXIT_DISCONNECTED,

    SCRCPY_EXIT_CONTINUE,
};

enum scrcpy_exit_code
scrcpy(struct scrcpy_options *options);


#ifdef _WIN32
#define SC_EXPORT __declspec(dllexport)
#else
#define SC_EXPORT __attribute__((visibility("default")))
#endif

SC_EXPORT int sc_run_as_dll_mode(const char* arginfo);


SC_EXPORT enum scrcpy_exit_code
sc_run_with_options(struct scrcpy_options *options);

SC_EXPORT enum scrcpy_exit_code
sc_step_mode_init(const char* work_directory,
                    int cli_service_port,
                    uint64_t parent_window_handle,
                    const char *serial, bool window, bool control, 
                   uint16_t max_size, uint32_t bit_rate, 
                   uint16_t max_fps, uint16_t window_width, 
                   uint16_t window_height, bool fullscreen,
                   bool show_touches, bool stay_awake, 
                   bool turn_screen_off, bool record_screen,
                   const char* record_filename);

// SC_EXPORT enum scrcpy_exit_code sc_step_mode_loop();
SC_EXPORT enum scrcpy_exit_code sc_step_mode_loop_once();
SC_EXPORT enum scrcpy_exit_code sc_step_mode_exit();

SC_EXPORT void sc_request_exit();

SC_EXPORT int sc_preinit_sdl();

#endif
