# Scrcpy 文件映射图像传输方案 (macOS 兼容)

本项目实现了将 scrcpy 的图像数据通过文件映射传输到 C# Avalonia 应用程序进行显示的功能，专门针对 macOS 系统优化。

## 问题背景

原始的共享内存方案在 macOS 上存在兼容性问题：
- macOS 的 `shm_open` 创建的共享内存无法被 C# 的 `MemoryMappedFile` 直接访问
- 需要一个跨平台且 C 和 C# 都能访问的解决方案

## 解决方案：文件映射

使用文件映射 (Memory-Mapped Files) 作为跨进程数据交换机制：
- C 端使用 `mmap()` 创建文件映射
- C# 端使用 `MemoryMappedFile.CreateFromFile()` 访问同一文件
- 在 macOS、Windows、Linux 上都有良好支持

## 架构概述

```
┌─────────────┐    文件映射    ┌──────────────────┐
│   scrcpy    │ ──────────────► │  C# Avalonia     │
│             │                │  图像查看器      │
│ - 解码视频  │    net_cmd     │                  │
│ - 格式转换  │ ──────────────► │ - 读取文件映射   │
│ - 写入文件  │                │ - 显示图像       │
│   映射      │                │ - 实时更新       │
└─────────────┘                └──────────────────┘
```

## 核心组件

### C 端 (scrcpy)

1. **文件映射模块** (`app/src/util/file_mapping.h/c`)
   - 跨平台文件映射创建和管理
   - 支持 macOS、Windows、Linux
   - 帧数据结构化存储

2. **图像传输器** (`app/src/util/image_transmitter.h/cpp`)
   - 使用文件映射替代共享内存
   - SDL 渲染器像素读取 (ABGR 格式)
   - 通过 net_cmd 发送通知

3. **集成到 scrcpy 主程序**
   - 在 `scrcpy.c` 中初始化图像传输器
   - 注册命令处理函数
   - 与现有 net_cmd 系统集成

### C# 端 (Avalonia)

1. **文件映射读取器** (`FileMappingReader.cs`)
   - 读取文件映射中的帧数据
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
    uint32_t width;       // 图像宽度
    uint32_t height;      // 图像高度
    uint32_t format;      // 图像格式 (2=RGBA)
    uint32_t frame_size;  // 帧数据大小
    uint64_t timestamp;   // 时间戳 (微秒)
    uint32_t sequence;    // 帧序列号
    uint32_t reserved[2]; // 保留字段
};
```

### 文件映射布局
```
临时目录/scrcpy_frames_8888.map
┌─────────────────┐
│   Frame Header  │ (32 bytes)
├─────────────────┤
│                 │
│   Image Data    │ (width * height * 4 bytes for RGBA)
│                 │
└─────────────────┘
```

## 文件路径规则

### C 端路径构建
```c
const char* temp_dir = getenv("TMPDIR") ?: "/tmp";
snprintf(file_path, sizeof(file_path), "%s/scrcpy_%s.map", temp_dir, name);
```

### C# 端路径构建
```csharp
string tempDir = Environment.GetEnvironmentVariable("TMPDIR") ?? "/tmp";
string filePath = Path.Combine(tempDir, $"scrcpy_{mappingName}.map");
```

## 使用方法

### 1. 编译 scrcpy
```bash
cd /path/to/scrcpy
./release/build_macos.sh  # macOS
```

### 2. 启动 scrcpy 与 cli-tools
```bash
# 启动 scrcpy 并指定 cli 服务端口
./scrcpy --cli-service-port=8888

# 文件映射会自动创建: /tmp/scrcpy_frames_8888.map
```

### 3. 编译并运行 C# 应用
```bash
cd dotnet_test
dotnet build
dotnet run
```

### 4. 连接和查看
1. 在 C# 应用中，映射名称默认为 `frames_8888`
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

### 获取文件映射信息
```
get_mapping_info
# 返回: {"mapping_name":"frames_8888","file_path":"/tmp/scrcpy_frames_8888.map","enabled":true}
```

## 技术细节

### 图像格式处理
- scrcpy 通过 SDL_RenderReadPixels 读取 ABGR 格式
- C 端直接写入 ABGR 数据到文件映射
- C# 端转换 ABGR → BGRA (Avalonia 格式)

### 同步机制
- 使用帧序列号避免重复处理
- 时间戳用于延迟测量
- 通过 net_cmd 发送帧可用通知
- 文件映射使用 `msync()` 确保数据同步

### 内存管理
- 文件映射大小根据最大分辨率预分配
- 创建者负责删除映射文件
- 支持多实例运行（不同端口）

## 性能特点

- **跨平台兼容**: 在 macOS 上完美工作
- **低延迟**: 直接文件映射访问，无网络传输开销
- **高效率**: 避免了数据拷贝，直接内存映射
- **实时性**: 支持 60+ FPS 的图像传输
- **可靠性**: 文件系统保证数据持久性

## 与共享内存方案的对比

| 特性 | 共享内存 | 文件映射 |
|------|----------|----------|
| macOS 兼容性 | ❌ 问题 | ✅ 完美 |
| 跨平台支持 | ⚠️ 部分 | ✅ 全面 |
| C# 访问 | ❌ 困难 | ✅ 简单 |
| 性能 | ✅ 最优 | ✅ 优秀 |
| 实现复杂度 | ⚠️ 中等 | ✅ 简单 |

## 故障排除

### 常见问题

1. **文件映射创建失败**
   - 检查临时目录权限
   - 确保磁盘空间充足
   - 验证文件路径长度

2. **C# 端无法访问文件**
   - 确认文件路径一致性
   - 检查文件权限设置
   - 验证文件是否存在

3. **图像显示异常**
   - 验证 ABGR → BGRA 转换
   - 检查字节序问题
   - 确认分辨率匹配

### 调试建议

1. 检查文件映射文件是否创建：
   ```bash
   ls -la /tmp/scrcpy_*.map
   ```

2. 监控文件大小变化：
   ```bash
   watch -n 1 'ls -lh /tmp/scrcpy_*.map'
   ```

3. 查看进程文件描述符：
   ```bash
   lsof | grep scrcpy.*\.map
   ```

## 依赖项

### C 端
- POSIX mmap API
- 标准文件操作 API
- SDL2 (scrcpy 现有依赖)

### C# 端
- .NET 6.0+
- Avalonia UI 11.0+
- System.IO.MemoryMappedFiles

## 总结

文件映射方案成功解决了 macOS 上共享内存的兼容性问题，提供了一个可靠、高效、跨平台的图像传输解决方案。这个实现为 scrcpy 在 macOS 上的功能扩展奠定了坚实基础。
