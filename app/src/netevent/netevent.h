#pragma once

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


struct netevent;
typedef void (*net_command_callback)(uint16_t, const char*, const char*, void*);


struct netevent *netevent_init(void);

void netevent_destroy(struct netevent *ne);

int netevent_connect(struct netevent *ne, const char *host, int port);


void netevent_register_command(const char* name, net_command_callback callback, void* userdata);


bool netevent_send_response(struct netevent *ne,
                            bool is_suc, 
                            uint16_t req_id,
                            const char *cmd,
                            const char *content);

bool netevent_send_request(struct netevent *ne,
    uint16_t req_id,
    const char *cmd,
    const char *content);

// 运行事件循环(非阻塞模式，适合主循环调用)
// 返回true表示还有事件待处理，false表示无事件
bool netevent_loop_once(struct netevent *ne);

// 停止事件循环
void netevent_stop(struct netevent *ne);

// 检查是否正在运行
bool netevent_is_running(struct netevent *ne);

void netevent_command_set_result(uint16_t req_id, uint8_t is_suc, const char* cmd, const char* result_info);


#ifdef __cplusplus
}
#endif

