# Scrcpy 图像传输到 C# Avalonia 显示功能

本项目实现了将 scrcpy 的图像数据通过共享内存传输到 C# Avalonia 应用程序进行显示的功能。

## 架构概述

```
┌─────────────┐    共享内存    ┌──────────────────┐
│   scrcpy    │ ──────────────► │  C# Avalonia     │
│             │                │  图像查看器      │
│ - 解码视频  │    net_cmd     │                  │
│ - 格式转换  │ ──────────────► │ - 读取共享内存   │
│ - 写入共享  │                │ - 显示图像       │
│   内存      │                │ - 实时更新       │
└─────────────┘                └──────────────────┘
```

## 核心组件

### C 端 (scrcpy)

1. **共享内存模块** (`app/src/util/shared_memory.h/c`)
   - 跨平台共享内存创建和管理
   - 支持 macOS、Windows、Linux
   - 帧数据结构化存储

2. **图像传输器** (`app/src/image_transmitter.h/c`)
   - YUV420P 到 RGB24 格式转换
   - 帧序列管理
   - 通过 net_cmd 发送通知

3. **集成到 scrcpy 主程序**
   - 在 `scrcpy.c` 中初始化图像传输器
   - 注册命令处理函数
   - 与现有 net_cmd 系统集成

### C# 端 (Avalonia)

1. **共享内存读取器** (`SharedMemoryReader.cs`)
   - 读取共享内存中的帧数据
   - 异步监控新帧
   - 事件驱动的帧通知

2. **主窗口和视图模型** (`MainWindow.axaml`, `MainWindowViewModel.cs`)
   - 实时图像显示
   - 连接状态管理
   - FPS 统计和帧信息显示

## 数据结构

### 帧头结构 (C/C#)
```c
struct sc_frame_header {
    int32_t width;        // 图像宽度
    int32_t height;       // 图像高度
    int32_t format;       // 图像格式 (1=RGB24)
    int32_t frame_size;   // 帧数据大小
    uint32_t sequence;    // 帧序列号
    uint64_t timestamp;   // 时间戳 (微秒)
    uint32_t reserved[2]; // 保留字段
};
```

### 共享内存布局
```
┌─────────────────┐
│   Frame Header  │ (32 bytes)
├─────────────────┤
│                 │
│   Image Data    │ (width * height * 3 bytes for RGB24)
│                 │
└─────────────────┘
```

## 使用方法

### 1. 编译 scrcpy
```bash
cd /path/to/scrcpy
# 使用现有的构建脚本
./release/build_macos.sh  # macOS
# 或
./release/build_windows.sh  # Windows
```

### 2. 启动 scrcpy 与 cli-tools
```bash
# 启动 scrcpy 并指定 cli 服务端口
./scrcpy --cli-service-port=8888

# 图像传输器会自动初始化，共享内存名称为: scrcpy_frames_8888
```

### 3. 编译并运行 C# 应用
```bash
cd /path/to/csharp/project
dotnet build
dotnet run
```

### 4. 连接和查看
1. 在 C# 应用中，共享内存名称默认为 `scrcpy_frames_8888`
2. 点击"连接"按钮
3. 通过 net_cmd 启用图像传输：
   ```bash
   # 发送命令启用图像传输
   echo "enable_image_transmitter:true" | nc localhost 8888
   ```

## 命令接口

通过 net_cmd 系统，支持以下命令：

### 启用/禁用图像传输
```
enable_image_transmitter:true   # 启用
enable_image_transmitter:false  # 禁用
```

### 获取共享内存信息
```
get_shared_memory_info
# 返回: {"shm_name":"scrcpy_frames_8888","shm_size":6220832,"enabled":true}
```

## 性能特点

- **低延迟**: 直接内存访问，无网络传输开销
- **高效率**: YUV 到 RGB 转换在 C 端完成
- **实时性**: 支持 60+ FPS 的图像传输
- **跨平台**: 支持 macOS、Windows、Linux

## 技术细节

### 图像格式转换
- scrcpy 解码得到 YUV420P 格式
- 图像传输器转换为 RGB24 格式
- C# 端转换为 Avalonia 需要的 BGRA 格式

### 同步机制
- 使用帧序列号避免重复处理
- 时间戳用于延迟测量
- 通过 net_cmd 发送帧可用通知

### 内存管理
- 共享内存大小根据最大分辨率预分配
- 自动清理和错误恢复
- 支持多实例运行（不同端口）

## 扩展可能

1. **支持更多图像格式**: JPEG、H.264 等
2. **添加图像处理**: 缩放、旋转、滤镜
3. **网络传输**: 支持远程显示
4. **录制功能**: 保存图像序列
5. **多设备支持**: 同时显示多个设备

## 故障排除

### 常见问题

1. **共享内存创建失败**
   - 检查权限设置
   - 确保名称唯一性
   - 验证内存大小限制

2. **图像显示异常**
   - 验证图像格式转换
   - 检查字节序问题
   - 确认分辨率匹配

3. **性能问题**
   - 调整帧率限制
   - 优化内存拷贝
   - 检查 CPU 使用率

### 调试建议

1. 启用详细日志输出
2. 使用内存分析工具
3. 监控共享内存状态
4. 检查网络命令响应

## 依赖项

### C 端
- SDL2 (scrcpy 现有依赖)
- FFmpeg (scrcpy 现有依赖)
- POSIX 共享内存 API (macOS/Linux)
- Windows API (Windows)

### C# 端
- .NET 6.0+
- Avalonia UI 11.0+
- System.IO.MemoryMappedFiles

这个实现提供了一个完整的图像传输解决方案，可以作为 scrcpy 功能扩展的基础。
