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


void net_cmd_register_command(const char* name, net_cmd_callback callback, void* userdata);


bool net_cmd_send_response(bool is_suc, 
                            uint16_t req_id,
                            const char *cmd,
                            const char *content);

uint16_t net_cmd_send_request(
    const char *cmd,
    const char *content);

void net_cmd_send_start_work_notify();

// 运行事件循环(非阻塞模式，适合主循环调用)
// 返回true表示还有事件待处理，false表示无事件
bool net_cmd_loop_once();

// 停止事件循环
void net_cmd_stop();

// 检查是否正在运行
bool net_cmd_is_running();

void net_cmd_set_last_result(uint16_t req_id, uint8_t is_suc, const char* cmd, const char* result_info);


#ifdef __cplusplus
}
#endif

