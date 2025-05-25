#ifndef SC_FILE_MAPPING_H
#define SC_FILE_MAPPING_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 文件映射结构体
struct sc_file_mapping {
    void *data;
    size_t size;
    char *file_path;
    int fd;
    bool is_creator;  // 是否是创建者
};

// 图像帧头信息
#pragma pack(push, 1)
struct sc_frame_header {
    uint32_t width;
    uint32_t height;
    uint32_t format;        // 像素格式 (YUV420P = 0, RGB24 = 1, RGBA = 2)
    uint32_t frame_size;    // 帧数据大小
    uint64_t timestamp;     // 时间戳
    uint32_t sequence;      // 序列号
    uint32_t reserved0;   // 保留字段
    uint32_t reserved1;   // 保留字段
};
#pragma pack(pop)

// 共享帧缓冲区结构
struct sc_shared_frame_buffer {
    struct sc_frame_header header;
    uint8_t frame_data[];   // 可变长度的帧数据
};

// 创建文件映射
bool sc_file_mapping_create(struct sc_file_mapping *mapping, const char *name, size_t size);

// 打开现有文件映射
bool sc_file_mapping_open(struct sc_file_mapping *mapping, const char *name, size_t size);

// 销毁文件映射
void sc_file_mapping_destroy(struct sc_file_mapping *mapping);

// 写入帧数据到文件映射
bool sc_file_mapping_write_frame(struct sc_file_mapping *mapping, 
                                 const struct sc_frame_header *header,
                                 const uint8_t *frame_data);

// 从文件映射读取帧数据
bool sc_file_mapping_read_frame(struct sc_file_mapping *mapping,
                                struct sc_frame_header *header,
                                uint8_t *frame_data,
                                size_t max_size);

// 获取临时目录路径
const char* sc_get_temp_dir(void);

#ifdef __cplusplus
}
#endif

#endif // SC_FILE_MAPPING_H
