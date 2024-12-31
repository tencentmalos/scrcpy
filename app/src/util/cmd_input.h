#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*input_command_callback)(const char*, void*);

void sc_start_cmd_input_thread();

void sc_stop_cmd_input_thread();

void sc_cmd_input_register_command(const char* name, input_command_callback callback, void* userdata);

void sc_cmd_input_loop_once();


#ifdef __cplusplus
}
#endif
