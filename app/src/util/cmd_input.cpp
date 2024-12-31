#include "cmd_input.h"

#include <pthread.h>
#include <unistd.h>

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

# ifdef _WIN32
#   include <windows.h>
#else
#   include <sys/select.h>
#endif

extern "C" {
#include "util/log.h"
}


static std::atomic<bool> g_running{true};
static std::shared_ptr<std::thread> g_cmd_input_thread;

struct input_command_info {
    std::string command_name;
    input_command_callback callback;
    void* userdata;
};


static std::map<std::string, input_command_info> g_input_command_map;

static std::mutex g_input_mutex;
static std::vector<std::string> g_cached_inputs;

#ifdef _WIN32

static OVERLAPPED g_ol = {};
static HANDLE g_handle_stdin;

void console_input_thread() {
 // 1. Get the standard input handle
    g_handle_stdin = GetStdHandle(STD_INPUT_HANDLE);
    if (g_handle_stdin == INVALID_HANDLE_VALUE) {
        LOGE("Failed to get STD_INPUT_HANDLE. Error: %d", GetLastError());
        return;
    }

    std::string cmd_buffer;
    g_ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  // Manual-reset event


    // 2. Main loop for polling console input
    while (g_running) {
        // 2.1 WaitForSingleObject to check if there's input available
        //     We'll wait 2 seconds (2000 ms) for demonstration
        DWORD waitResult = WaitForSingleObject(g_handle_stdin, 100);

        switch (waitResult) {
            case WAIT_OBJECT_0:
            {
                // There is input available; let's read it
                // We don't know how many bytes yet, so let's do a simple ReadFile
                // with a reasonably sized buffer.

                // For console input, the user typically presses ENTER to submit,
                // so the read might include the newline or may read partial data
                // depending on console mode settings.

                CHAR buffer[256] = {0};
                DWORD bytesRead = 0;
                BOOL success = ReadFile(
                    g_handle_stdin,       // handle to stdin
                    buffer,       // buffer to receive data
                    sizeof(buffer) - 1, // max bytes we can read
                    &bytesRead,   // actual bytes read
                    &g_ol          // no OVERLAPPED struct here
                );

                if (!success) {
                    std::cerr << "ReadFile failed. Error: " 
                              << GetLastError() << std::endl;
                    // In real code, handle error or exit
                    break;
                }

                // Null-terminate to make it a C-string for printing
                buffer[bytesRead] = '\0';

                 // Accumulate into cmdBuffer
                for (DWORD i = 0; i < bytesRead; i++) {
                    char c = buffer[i];
                    if (c == '\r') {
                        // Do nothing or handle \r
                    } else if (c == '\n') {
                        // User pressed Enter => full command read
                        LOGI("Command: %s", cmd_buffer.c_str());
                        {
                            std::lock_guard<std::mutex> tmp_lock(g_input_mutex);
                            g_cached_inputs.emplace_back(cmd_buffer);
                        }
                        cmd_buffer.clear();
                    } else {
                        cmd_buffer.push_back(c);
                    }
                }
            }
            break;

            case WAIT_TIMEOUT:
                // No input within 2 seconds, do something else...
                // std::cout << "[No input, do other work...]\n";
                // We'll just continue the loop
                break;

            case WAIT_FAILED:
                LOGE("WaitForSingleObject failed. Error: %d", GetLastError());
                g_running = false;
                break;

            default:
                // Should not happen normally, but handle it anyway
                LOGE("Unexpected waitResult: %d", waitResult);
                g_running = false;
                break;
        }

        // 2.2 Do other periodic tasks if needed...
        // For demonstration, just continue the loop until we decide to exit
    }
}
#else
void console_input_thread() {
    while (g_running) {
        // Set up file descriptor set
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        // Set up timeout (5 seconds)
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100* 1000;    //100ms

        // Use select to wait for stdin to be readable or timeout
        int ret = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
        if (ret == -1) {
            perror("select() error");
            break;
        } else if (ret == 0) {
            // Timeout
            continue;
        } 
        else 
        {
            std::string line;
            if (!std::getline(std::cin, line)) {
                std::cout << "EOF or cin error. Exiting input loop.\n";
                g_running = false;
                break;
            }

            if (line.empty()) {
                continue;
            }

            {
                std::lock_guard<std::mutex> tmp_lock(g_input_mutex);
                g_cached_inputs.emplace_back(line);
            }
        }
    }
}
#endif




extern "C" {

void sc_start_cmd_input_thread() {
    g_cmd_input_thread = std::make_shared<std::thread>(console_input_thread);
}

void sc_stop_cmd_input_thread() {
    if(g_cmd_input_thread) {
        g_running = false;
#ifdef _WIN32
        CancelIoEx(g_handle_stdin, &g_ol);
#endif
        if (g_cmd_input_thread->joinable()) {
            g_cmd_input_thread->join();
        }
        g_cmd_input_thread.reset();
    }
}

void sc_cmd_input_register_command(const char* name, input_command_callback callback, void* userdata) {
    //ToDo: add implement here
    std::string cmdname = name;
    LOGI("input command: %s registered!", name);

    input_command_info info;
    info.command_name = cmdname;
    info.callback = callback;
    info.userdata = userdata;

    g_input_command_map.emplace(std::make_pair(cmdname, info));
}

void sc_cmd_input_loop_once() {
    std::vector<std::string> tmplines;
    {
        std::lock_guard<std::mutex> tmp_lock(g_input_mutex);
        tmplines.swap(g_cached_inputs);
    }

    for(auto line: tmplines) {
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
            it->second.callback(part2.c_str(), it->second.userdata);
            LOGI("> %s %s", cmdname.c_str(), extras.c_str());
        } else {
            LOGE("Unknown command: %s, extra: %s", cmdname.c_str(), extras.c_str());
        }
    }
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

