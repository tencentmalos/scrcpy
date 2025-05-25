#ifndef SC_SHARED_MEMORY_H
#define SC_SHARED_MEMORY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 共享内存结构体
struct sc_shared_memory {
    void *data;
    size_t size;
    char *name;
#ifdef __APPLE__
    int fd;
#elif defined(_WIN32)
    void *handle;
#else
    int fd;
#endif
};

// 图像帧头信息
struct sc_frame_header {
    uint32_t width;
    uint32_t height;
    uint32_t format;        // 像素格式 (YUV420P = 0, RGB24 = 1, RGBA = 2)
    uint32_t frame_size;    // 帧数据大小
    uint64_t timestamp;     // 时间戳
    uint32_t sequence;      // 序列号
    uint32_t reserved[2];   // 保留字段
};

// 共享内存缓冲区结构
struct sc_shared_frame_buffer {
    struct sc_frame_header header;
    uint8_t frame_data[];   // 可变长度的帧数据
};

// 创建共享内存
bool sc_shared_memory_create(struct sc_shared_memory *shm, const char *name, size_t size);

// 打开现有共享内存
bool sc_shared_memory_open(struct sc_shared_memory *shm, const char *name, size_t size);

// 销毁共享内存
void sc_shared_memory_destroy(struct sc_shared_memory *shm);

// 写入帧数据到共享内存
bool sc_shared_memory_write_frame(struct sc_shared_memory *shm, 
                                  const struct sc_frame_header *header,
                                  const uint8_t *frame_data);

// 从共享内存读取帧数据
bool sc_shared_memory_read_frame(struct sc_shared_memory *shm,
                                 struct sc_frame_header *header,
                                 uint8_t *frame_data,
                                 size_t max_size);

#ifdef __cplusplus
}
#endif

#endif // SC_SHARED_MEMORY_H
