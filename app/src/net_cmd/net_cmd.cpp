#include "net_cmd.h"

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
    #include "scrcpy.h"
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
struct net_cmd_state {
    struct event_base *base;
    struct bufferevent *bev;
    //void (*on_command)(const char *cmd, const char *content, void *userdata);
    void *userdata;
    bool running;

    //logic data here?
};

//static bool g_netevent_running = true;

struct netevent_command_info {
    std::string             command_name;
    net_cmd_callback        callback;
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


net_cmd_state* g_net_state = nullptr;
uint16_t g_net_request_id_count = 0;


bool net_cmd_init(void) {
    if(g_net_state) {
        LOGW("net_cmd is already work here!");
        return true;
    }

    struct net_cmd_state *ne = (struct net_cmd_state *)calloc(1, sizeof(*ne));
    if (!ne) return false;

    ne->base = event_base_new();
    if (!ne->base) {
        free(ne);
        return false;
    }

    ne->running = false;

    g_net_state = ne;

    return true;
}

void net_cmd_destroy() {
    if (!g_net_state) return;

    if (g_net_state->bev) bufferevent_free(g_net_state->bev);
    if (g_net_state->base) event_base_free(g_net_state->base);
    free(g_net_state);

    g_net_state = nullptr;
}

static void netevent_send_last_result() {
    net_cmd_send_response(g_netevent_response_info.is_suc,
        g_netevent_response_info.request_id,
        g_netevent_response_info.cmd_name.c_str(),
        g_netevent_response_info.response_result.c_str());
}


// command handle function here, now in main thread, so just call the command is ok.
static void on_netevent_command(uint16_t req_id, const char *cmd, const char *content, void *userdata) {
    std::string cmdname = cmd != nullptr ? cmd : "";
    std::string extras = content != nullptr ? content: "";

    LOGD("net_cmd <- [%s] %s (%d)", cmdname.c_str(), extras.c_str(), (int)req_id);

    auto it = g_netevent_command_map.find(cmdname);
    if (it != g_netevent_command_map.end()) {
        //default is suc here        
        net_cmd_set_last_result(req_id, 1, cmd, "");
        it->second.callback(req_id, cmd, content, it->second.userdata);
    } else {
        net_cmd_set_last_result(req_id, 0, cmd, "unknown");
        //LOGI("icmd-unknown-0: %s %s", cmdname.c_str(), extras.c_str());
    }

    netevent_send_last_result();

    LOGD("Scrcpy execute command:[%s] %s", cmd, content);
}


static void read_cb(struct bufferevent *bev, void *ctx) {
    struct net_cmd_state *ne = (struct net_cmd_state *)ctx;
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
    struct net_cmd_state *ne = (struct net_cmd_state *)ctx;
    if (events & BEV_EVENT_ERROR) {

        //on_netevent_command(1, "error", "connection error", ne->userdata);
        // if (ne->on_command) {
        //     ne->on_command("error", "connection error", ne->userdata);
        // }

        bufferevent_free(bev);
        ne->bev = nullptr;

        LOGE("cli service is run with connection error here, scrcpy just exited now.");
        //sc_request_exit();
        exit(-1);   //just force kill here
    }
}

int net_cmd_connect(const char *host, int port) {
    if (!g_net_state || !host || port <= 0) return -1;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    evutil_inet_pton(AF_INET, host, &sin.sin_addr);

    g_net_state->bev = bufferevent_socket_new(g_net_state->base, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!g_net_state->bev) return -1;

    bufferevent_setcb(g_net_state->bev, read_cb, nullptr, event_cb, g_net_state);
    bufferevent_enable(g_net_state->bev, EV_READ|EV_WRITE);

    if (bufferevent_socket_connect(g_net_state->bev, 
                                 (struct sockaddr*)&sin, 
                                 sizeof(sin))) {
        bufferevent_free(g_net_state->bev);
        g_net_state->bev = nullptr;
        return -1;
    }

    return 0;
}

void net_cmd_register_command(const char* name, net_cmd_callback callback, void* userdata) {
    //ToDo: add implement here
    std::string cmdname = name;
    LOGI("netevent command: %s registered!", name);

    netevent_command_info info;
    info.command_name = cmdname;
    info.callback = callback;
    info.userdata = userdata;

    g_netevent_command_map.insert(std::make_pair(cmdname, info));
}



bool net_cmd_send_response(bool is_suc,
                            uint16_t req_id,
                            const char *cmd,
                            const char *content) {
    if (!g_net_state || !g_net_state->bev || !cmd) return false;

    netevent_header header;
    header.type = 1;
    header.flag = is_suc? 1: 0;
    header.id = htons(req_id);
    header.cmd_len = htonl(strlen(cmd));
    header.content_len = content ? htonl(strlen(content)) : 0;
    //header.reserved = 0;
    
    struct evbuffer *output = bufferevent_get_output(g_net_state->bev);
    evbuffer_add(output, &header, sizeof(header));
    evbuffer_add(output, cmd, strlen(cmd));
    if (content) {
        evbuffer_add(output, content, strlen(content));
    }
    return true;
}

uint16_t net_cmd_send_request(const char *cmd,
                            const char *content) {
    if (!g_net_state || !g_net_state->bev || !cmd) return 0;

    uint16_t req_id = ++g_net_request_id_count;
    
    netevent_header header;
    header.type = 0;
    header.flag = 0;
    header.id = htons(req_id);
    header.cmd_len = htonl(strlen(cmd));
    header.content_len = content ? htonl(strlen(content)) : 0;
    //header.reserved = 0;
    
    struct evbuffer *output = bufferevent_get_output(g_net_state->bev);
    evbuffer_add(output, &header, sizeof(header));
    evbuffer_add(output, cmd, strlen(cmd));
    if (content) {
        evbuffer_add(output, content, strlen(content));
    }
    return req_id;
}

void net_cmd_send_start_work_notify() {
    net_cmd_send_request("start_work_notify", "");
}

bool net_cmd_loop_once() {
    if (!g_net_state || !g_net_state->base) return false;
    
    int flags = EVLOOP_ONCE | EVLOOP_NONBLOCK;

    int res = event_base_loop(g_net_state->base, flags);
    
    // 返回true表示还有事件待处理
    return res == 0; 
}

// 停止事件循环
void net_cmd_stop() {
    if (!g_net_state || !g_net_state->base) return;
    event_base_loopbreak(g_net_state->base);
}

// 检查是否正在运行
bool net_cmd_is_running() {
    return g_net_state && g_net_state->base && !event_base_got_break(g_net_state->base);
}


void net_cmd_set_last_result(uint16_t req_id, uint8_t is_suc, const char* cmd, const char* result_info) {
    g_netevent_response_info.cmd_name = cmd;
    g_netevent_response_info.request_id = req_id;
    g_netevent_response_info.is_suc = is_suc;
    g_netevent_response_info.response_result = result_info;
}



