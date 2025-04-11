#include "netevent.h"
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <string.h>
#include <stdlib.h>
//#include <arpa/inet.h>

struct netevent {
    struct event_base *base;
    struct bufferevent *bev;
    void (*on_command)(const char *cmd, const char *content, void *userdata);
    void *userdata;
    bool running;
};

//netevent header
#pragma pack(push, 1)
typedef struct {
    uint16_t cmd_len;    // cmd字段长度
    uint16_t content_len; // content字段长度 
    //uint32_t reserved;    // 保留字段
} netevent_header;
#pragma pack(pop)

// 初始化libevent客户端模块
struct netevent *netevent_init(void) {
    struct netevent *ne = calloc(1, sizeof(*ne));
    if (!ne) return NULL;

    ne->base = event_base_new();
    if (!ne->base) {
        free(ne);
        return NULL;
    }

    ne->running = false;
    return ne;
}

// 清理资源
void netevent_destroy(struct netevent *ne) {
    if (!ne) return;

    if (ne->bev) bufferevent_free(ne->bev);
    if (ne->base) event_base_free(ne->base);
    free(ne);
}

// 读取回调
static void read_cb(struct bufferevent *bev, void *ctx) {
    struct netevent *ne = ctx;
    struct evbuffer *input = bufferevent_get_input(bev);
    
    while (1) {
        // 1. 检查是否有完整包头
        if (evbuffer_get_length(input) < sizeof(netevent_header)) {
            return;
        }
        
        // 2. 读取包头
        netevent_header header;
        evbuffer_copyout(input, &header, sizeof(header));
        header.cmd_len = ntohs(header.cmd_len);
        header.content_len = ntohs(header.content_len);
        
        // 3. 检查是否有完整数据包
        size_t total_len = sizeof(header) + header.cmd_len + header.content_len;
        if (evbuffer_get_length(input) < total_len) {
            return;
        }
        
        // 4. 移除包头
        evbuffer_drain(input, sizeof(header));
        
        // 5. 读取cmd和content
        char *cmd = malloc(header.cmd_len + 1);
        char *content = header.content_len > 0 ? malloc(header.content_len + 1) : NULL;
        
        evbuffer_remove(input, cmd, header.cmd_len);
        cmd[header.cmd_len] = '\0';
        
        if (content) {
            evbuffer_remove(input, content, header.content_len);
            content[header.content_len] = '\0';
        }
        
        // 6. 回调处理
        if (ne->on_command) {
            ne->on_command(cmd, content, ne->userdata);
        }
        
        free(cmd);
        if (content) free(content);
    }
}

// 事件回调
static void event_cb(struct bufferevent *bev, short events, void *ctx) {
    struct netevent *ne = ctx;
    if (events & BEV_EVENT_ERROR) {
        if (ne->on_command) {
            ne->on_command("error", "connection error", ne->userdata);
        }
        bufferevent_free(bev);
        ne->bev = NULL;
    }
}

// 连接到远程服务器
int netevent_connect(struct netevent *ne, const char *host, int port) {
    if (!ne || !host || port <= 0) return -1;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    evutil_inet_pton(AF_INET, host, &sin.sin_addr);

    ne->bev = bufferevent_socket_new(ne->base, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!ne->bev) return -1;

    bufferevent_setcb(ne->bev, read_cb, NULL, event_cb, ne);
    bufferevent_enable(ne->bev, EV_READ|EV_WRITE);

    if (bufferevent_socket_connect(ne->bev, 
                                 (struct sockaddr*)&sin, 
                                 sizeof(sin))) {
        bufferevent_free(ne->bev);
        ne->bev = NULL;
        return -1;
    }

    return 0;
}

// 设置命令响应回调
void netevent_set_command_handler(struct netevent *ne,
                                void (*on_command)(const char *cmd,
                                                 const char *content,
                                                 void *userdata),
                                void *userdata) {
    if (!ne) return;
    ne->on_command = on_command;
    ne->userdata = userdata;
}

// 发送响应到服务器
bool netevent_send_response(struct netevent *ne, 
                          const char *cmd,
                          const char *content) {
    if (!ne || !ne->bev || !cmd) return false;

    netevent_header header;
    header.cmd_len = htons(strlen(cmd));
    header.content_len = content ? htons(strlen(content)) : 0;
    //header.reserved = 0;
    
    struct evbuffer *output = bufferevent_get_output(ne->bev);
    evbuffer_add(output, &header, sizeof(header));
    evbuffer_add(output, cmd, strlen(cmd));
    if (content) {
        evbuffer_add(output, content, strlen(content));
    }
    return true;
}

// 运行事件循环
void netevent_run(struct netevent *ne) {
    if (!ne || !ne->base) return;
    ne->running = true;
    event_base_dispatch(ne->base);
}

// 停止事件循环
void netevent_stop(struct netevent *ne) {
    if (!ne || !ne->base) return;
    ne->running = false;
    event_base_loopbreak(ne->base);
}
