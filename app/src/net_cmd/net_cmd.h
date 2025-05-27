#pragma once

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


//struct netevent;
typedef void (*net_cmd_callback)(uint16_t, const char*, const char*, void*);


bool net_cmd_init(void);

void net_cmd_destroy();

int net_cmd_connect(const char *host, int port);


void net_cmd_register_command(const char* name, net_cmd_callback callback, void* userdata, bool need_response);


bool net_cmd_send_response(bool is_suc, 
                            uint16_t req_id,
                            const char *cmd,
                            const char *content);

uint16_t net_cmd_send_request(
    const char *cmd,
    const char *content);

uint16_t net_cmd_send_request_fmt(
    const char *cmd,
    const char *format, ...);

void net_cmd_send_check_alive();

void net_cmd_send_start_to_work();

// Run event loop (non-blocking mode, suitable for main loop call)
// Returns true if there are events to process, false otherwise
bool net_cmd_loop_once();

// Stop event loop
void net_cmd_stop();

// Check if running
bool net_cmd_is_running();

void net_cmd_set_last_result(uint16_t req_id, uint8_t is_suc, const char* cmd, const char* result_info);

void net_cmd_set_last_result_ignore();

int64_t net_cmd_query_now_time_ms();

int64_t net_cmd_query_now_time_us();

void net_cmd_set_current_fps(uint32_t fps, uint32_t skiped_fps);

void net_cmd_redirect_log_to_network();

#ifdef __cplusplus
}
#endif
