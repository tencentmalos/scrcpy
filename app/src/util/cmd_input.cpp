#include "cmd_input.h"

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


extern "C" {
#include "util/log.h"
}


static std::atomic<bool> g_running{true};
static std::shared_ptr<std::thread> g_cmd_input_thread;

static std::map<std::string, input_command_callback> g_input_command_map;


void console_input_thread() {
    while (g_running) {
        std::cout << "> ";  
        
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "EOF or cin error. Exiting input loop.\n";
            g_running = false;
            break;
        }

        if (line.empty()) {
            continue;
        }

        // Use istringstream to parse the string
        std::istringstream iss(line);

        // part1 is extracted by operator>>
        std::string part1;
        iss >> part1; 

        // The rest of the string will be extracted by getline
        std::string part2;
        std::getline(iss, part2);

        // Remove leading spaces/tabs in part2
        if (!part2.empty()) {
            size_t start_pos = part2.find_first_not_of(" \t");
            if (start_pos != std::string::npos) {
                part2 = part2.substr(start_pos);
            } else {
                part2.clear();
            }
        }

        std::string cmdname = part1;
        std::string extras = part2;

        auto it = g_input_command_map.find(cmdname);
        if (it != g_input_command_map.end()) {
            it->second(part2.c_str());
        } else {
            LOGE("Unknown command: %s, extra: %s", cmdname.c_str(), extras.c_str());
        }
    }
}


extern "C" {

void sc_start_cmd_input_thread() {
    g_cmd_input_thread = std::make_shared<std::thread>(console_input_thread);
}

void sc_stop_cmd_input_thread() {
    if(g_cmd_input_thread) {
         g_running = false;
        // if (g_cmd_input_thread->joinable()) {
        //     g_cmd_input_thread->join();
        // }
        g_cmd_input_thread.reset();
    }
}

void sc_cmd_input_register_command(const char* name, input_command_callback callback) {
    //ToDo: add implement here
    std::string cmdname = name;
    LOGI("input command: %s registered!", name);

    g_input_command_map[cmdname] = callback;
}


}





// int main() {
//     // 注册一些命令 (示例)
//     gCommands["enable_feature"] = [](const std::vector<std::string>& args){
//         std::cout << "[CMD] enable_feature called.\n";
//         // 在这里实现真正启用功能的逻辑...
//     };

//     gCommands["disable_feature"] = [](const std::vector<std::string>& args){
//         std::cout << "[CMD] disable_feature called.\n";
//     };

//     gCommands["quit"] = [](const std::vector<std::string>& args){
//         std::cout << "[CMD] quit called. Exiting.\n";
//         gRunning = false;  // 让输入线程循环退出
//     };

//     // 启动子线程, 用于处理控制台输入
//     std::thread t(consoleInputThread);

//     // 主线程做点别的事 (模拟), 这里是个循环倒计时
//     for (int i = 5; i > 0; --i) {
//         std::cout << "[Main Thread] Doing work... (" << i << ")\n";
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//         if (!gRunning) {
//             break; // 如果输入线程提前让gRunning=false, 主线程可提前结束
//         }
//     }

//     // 如果主线程决定结束程序, 可以把gRunning=false
//     std::cout << "[Main Thread] Setting gRunning=false, joining input thread...\n";
//     gRunning = false;

//     // 等待输入线程结束
//     if (t.joinable()) {
//         t.join();
//     }

//     std::cout << "Program exiting.\n";
//     return 0;
// }

