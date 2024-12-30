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


static std::atomic<bool> g_running{true};
static std::shared_ptr<std::thread> g_cmd_input_thread;


using CommandFn = std::function<void(const std::vector<std::string>&)>;

std::map<std::string, CommandFn> gCommands;


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

        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        const std::string& cmdName = tokens[0];
        std::vector<std::string> cmdArgs;
        if (tokens.size() > 1) {
            cmdArgs.assign(tokens.begin() + 1, tokens.end());
        }

        auto it = gCommands.find(cmdName);
        if (it != gCommands.end()) {
            it->second(cmdArgs);
        } else {
            std::cout << "[Error] Unknown command: " << cmdName << std::endl;
        }
    }
}


#ifdef __cplusplus
extern "C" {
#endif

void sc_start_cmd_input_thread() {
    g_cmd_input_thread = std::make_shared<std::thread>(console_input_thread);
}

void sc_stop_cmd_input_thread() {
    if(g_cmd_input_thread) {
         g_running = false;
        if (g_cmd_input_thread->joinable()) {
            g_cmd_input_thread->join();
        }
        g_cmd_input_thread.reset();
    }
}

#ifdef __cplusplus
}
#endif




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

