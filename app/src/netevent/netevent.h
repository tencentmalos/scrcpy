#ifndef SCRCPY_NETEVENT_H
#define SCRCPY_NETEVENT_H

#include <stdbool.h>
#include <stdint.h>
#include <event2/event.h>

struct netevent;

// 初始化libevent客户端模块
struct netevent *netevent_init(void);

// 清理资源
void netevent_destroy(struct netevent *ne);

// 连接到远程服务器
int netevent_connect(struct netevent *ne, const char *host, int port);

// 设置命令响应回调(cmd和content两个参数)
void netevent_set_command_handler(struct netevent *ne,
                                void (*on_command)(const char *cmd,
                                                 const char *content,
                                                 void *userdata),
                                void *userdata);

// 发送响应到服务器(包含cmd和content)
bool netevent_send_response(struct netevent *ne, 
                          const char *cmd,
                          const char *content);

// 运行事件循环
void netevent_run(struct netevent *ne);

// 停止事件循环
void netevent_stop(struct netevent *ne);

#endif // SCRCPY_NETEVENT_H
