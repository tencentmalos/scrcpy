#include "netevent.h"

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <string.h>
#include <stdlib.h>


#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <map>
#include <functional>
#include <vector>
#include <sstream>
#include <chrono>
#include <memory>
#include <mutex>

extern "C" {
    #include "util/log.h"
}
    

//#include <arpa/inet.h>

//packet head
#pragma pack(push, 1)
typedef struct {
    uint8_t  type;          // 0 for request, 1 for response
    uint8_t  flag;          // when type = 1, flag is use as suc or failed here
    uint16_t id;            // flow id
    uint32_t cmd_len;       // cmd string length
    uint32_t content_len;   // content string length
} netevent_header;
#pragma pack(pop)


//netevent header
struct netevent {
    struct event_base *base;
    struct bufferevent *bev;
    //void (*on_command)(const char *cmd, const char *content, void *userdata);
    void *userdata;
    bool running;

    //logic data here?
};


netevent* g_netevent = nullptr;

//static bool g_netevent_running = true;

struct netevent_command_info {
    std::string             command_name;
    net_command_callback    callback;
    void*                   userdata;
};

static std::map<std::string, netevent_command_info> g_netevent_command_map;

//static std::mutex g_input_mutex;
//static std::vector<std::string> g_cached_inputs;

struct netevent_response_info
{
    std::string cmd_name;
    uint16_t    request_id;
    uint8_t     is_suc;
    std::string response_result;
};


netevent_response_info g_netevent_response_info;


struct netevent *netevent_init(void) {
    struct netevent *ne = (struct netevent *)calloc(1, sizeof(*ne));
    if (!ne) return NULL;

    ne->base = event_base_new();
    if (!ne->base) {
        free(ne);
        return NULL;
    }

    ne->running = false;

    g_netevent = ne;

    return ne;
}

void netevent_destroy(struct netevent *ne) {
    if (!ne) return;

    if (ne->bev) bufferevent_free(ne->bev);
    if (ne->base) event_base_free(ne->base);
    free(ne);

    g_netevent = nullptr;
}

static void netevent_send_last_result() {
    netevent_send_response(g_netevent, 
        g_netevent_response_info.is_suc,
        g_netevent_response_info.request_id,
        g_netevent_response_info.cmd_name.c_str(),
        g_netevent_response_info.response_result.c_str());
}


// command handle function here, now in main thread, so just call the command is ok.
static void on_netevent_command(uint16_t req_id, const char *cmd, const char *content, void *userdata) {
    std::string cmdname = cmd;
    std::string extras = content;

    LOGI("> netevent: %d [%s] %s", (int)req_id, cmdname.c_str(), extras.c_str());

    auto it = g_netevent_command_map.find(cmdname);
    if (it != g_netevent_command_map.end()) {
        //default is suc here        
        netevent_command_set_result(req_id, 1, cmd, "");
        it->second.callback(req_id, cmd, content, it->second.userdata);
    } else {
        netevent_command_set_result(req_id, 0, cmd, "unknown");
        //LOGI("icmd-unknown-0: %s %s", cmdname.c_str(), extras.c_str());
    }

    netevent_send_last_result();

    LOGI("Scrcpy execute command:[%s] %s", cmd, content);
}


static void read_cb(struct bufferevent *bev, void *ctx) {
    struct netevent *ne = (struct netevent *)ctx;
    struct evbuffer *input = bufferevent_get_input(bev);
    
    while (1) {
        // 1. 检查是否有完整包头
        if (evbuffer_get_length(input) < sizeof(netevent_header)) {
            return;
        }
        
        // 2. 读取包头
        netevent_header header;
        evbuffer_copyout(input, &header, sizeof(header));
        header.id = (uint16_t)htons((int16_t)header.id);
        header.cmd_len = htonl(header.cmd_len);
        header.content_len = htonl(header.content_len);
        
        // 3. 检查是否有完整数据包
        size_t total_len = sizeof(header) + header.cmd_len + header.content_len;
        if (evbuffer_get_length(input) < total_len) {
            return;
        }
        
        // 4. 移除包头
        evbuffer_drain(input, sizeof(header));
        
        // 5. 读取cmd和content
        char *cmd = (char*)malloc(header.cmd_len + 1);
        char *content = header.content_len > 0 ? (char*)malloc(header.content_len + 1) : NULL;
        
        evbuffer_remove(input, cmd, header.cmd_len);
        cmd[header.cmd_len] = '\0';
        
        if (content) {
            evbuffer_remove(input, content, header.content_len);
            content[header.content_len] = '\0';
        }
        
        // 6. 回调处理
        on_netevent_command(header.id, cmd, content, ne->userdata);
        // if (ne->on_command) {
        //     ne->on_command(cmd, content, ne->userdata);
        // }
        
        free(cmd);
        if (content) free(content);
    }
}

// 事件回调
static void event_cb(struct bufferevent *bev, short events, void *ctx) {
    struct netevent *ne = (struct netevent *)ctx;
    if (events & BEV_EVENT_ERROR) {

        on_netevent_command(1, "error", "connection error", ne->userdata);
        // if (ne->on_command) {
        //     ne->on_command("error", "connection error", ne->userdata);
        // }

        bufferevent_free(bev);
        ne->bev = NULL;
    }
}

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

void netevent_register_command(const char* name, net_command_callback callback, void* userdata) {
    //ToDo: add implement here
    std::string cmdname = name;
    LOGI("netevent command: %s registered!", name);

    netevent_command_info info;
    info.command_name = cmdname;
    info.callback = callback;
    info.userdata = userdata;

    g_netevent_command_map.insert(std::make_pair(cmdname, info));
}



bool netevent_send_response(struct netevent *ne,
                            bool is_suc,
                            uint16_t req_id,
                            const char *cmd,
                            const char *content) {
    if (!ne || !ne->bev || !cmd) return false;

    netevent_header header;
    header.type = 1;
    header.flag = is_suc? 1: 0;
    header.id = htons(req_id);
    header.cmd_len = htonl(strlen(cmd));
    header.content_len = content ? htonl(strlen(content)) : 0;
    //header.reserved = 0;
    
    struct evbuffer *output = bufferevent_get_output(ne->bev);
    evbuffer_add(output, &header, sizeof(header));
    evbuffer_add(output, cmd, strlen(cmd));
    if (content) {
        evbuffer_add(output, content, strlen(content));
    }
    return true;
}

bool netevent_send_request(struct netevent *ne,
                            uint16_t req_id,
                            const char *cmd,
                            const char *content) {
    if (!ne || !ne->bev || !cmd) return false;

    netevent_header header;
    header.type = 0;
    header.flag = 0;
    header.id = htonl(req_id);
    header.cmd_len = htonl(strlen(cmd));
    header.content_len = content ? htonl(strlen(content)) : 0;
    //header.reserved = 0;
    
    struct evbuffer *output = bufferevent_get_output(ne->bev);
    evbuffer_add(output, &header, sizeof(header));
    evbuffer_add(output, cmd, strlen(cmd));
    if (content) {
        evbuffer_add(output, content, strlen(content));
    }
    return true;
}

bool netevent_loop_once(struct netevent *ne) {
    if (!ne || !ne->base) return false;
    
    int flags = EVLOOP_ONCE | EVLOOP_NONBLOCK;

    int res = event_base_loop(ne->base, flags);
    
    // 返回true表示还有事件待处理
    return res == 0; 
}

// 停止事件循环
void netevent_stop(struct netevent *ne) {
    if (!ne || !ne->base) return;
    event_base_loopbreak(ne->base);
}

// 检查是否正在运行
bool netevent_is_running(struct netevent *ne) {
    return ne && ne->base && !event_base_got_break(ne->base);
}


void netevent_command_set_result(uint16_t req_id, uint8_t is_suc, const char* cmd, const char* result_info) {
    g_netevent_response_info.cmd_name = cmd;
    g_netevent_response_info.request_id = req_id;
    g_netevent_response_info.is_suc = is_suc;
    g_netevent_response_info.response_result = result_info;
}



