#include "file_mapping.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#include "util/log.h"

const char* sc_get_temp_dir(void) {
    const char* temp_dir = getenv("TMPDIR");
    if (temp_dir) {
        return temp_dir;
    }
    
    temp_dir = getenv("TMP");
    if (temp_dir) {
        return temp_dir;
    }
    
    temp_dir = getenv("TEMP");
    if (temp_dir) {
        return temp_dir;
    }
    
    return "/tmp/";
}

bool sc_file_mapping_create(struct sc_file_mapping *mapping, const char *name, size_t size) {
    if (!mapping || !name || size == 0) {
        return false;
    }

    memset(mapping, 0, sizeof(*mapping));
    mapping->size = size;
    mapping->is_creator = true;
    
    // 构建文件路径
    const char* temp_dir = sc_get_temp_dir();
    size_t path_len = strlen(temp_dir) + strlen(name) + 20;
    mapping->file_path = malloc(path_len);
    if (!mapping->file_path) {
        return false;
    }
    
    snprintf(mapping->file_path, path_len, "%sscrcpy_%s.map", temp_dir, name);
    
    // 创建文件
    mapping->fd = open(mapping->file_path, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (mapping->fd == -1) {
        LOGE("Failed to create mapping file: %s, error: %s", mapping->file_path, strerror(errno));
        free(mapping->file_path);
        return false;
    }
    
    // 设置文件大小
    if (ftruncate(mapping->fd, size) == -1) {
        LOGE("Failed to set file size: %s", strerror(errno));
        close(mapping->fd);
        unlink(mapping->file_path);
        free(mapping->file_path);
        return false;
    }
    
    // 映射文件到内存
    mapping->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mapping->fd, 0);
    if (mapping->data == MAP_FAILED) {
        LOGE("Failed to map file to memory: %s", strerror(errno));
        close(mapping->fd);
        unlink(mapping->file_path);
        free(mapping->file_path);
        return false;
    }
    
    // 初始化内存为零
    memset(mapping->data, 0, size);
    
    LOGI("Created file mapping: %s, size: %zu", mapping->file_path, size);
    return true;
}

bool sc_file_mapping_open(struct sc_file_mapping *mapping, const char *name, size_t size) {
    if (!mapping || !name || size == 0) {
        return false;
    }

    memset(mapping, 0, sizeof(*mapping));
    mapping->size = size;
    mapping->is_creator = false;
    
    // 构建文件路径
    const char* temp_dir = sc_get_temp_dir();
    size_t path_len = strlen(temp_dir) + strlen(name) + 20;
    mapping->file_path = malloc(path_len);
    if (!mapping->file_path) {
        return false;
    }
    
    snprintf(mapping->file_path, path_len, "%s/scrcpy_%s.map", temp_dir, name);
    
    // 打开现有文件
    mapping->fd = open(mapping->file_path, O_RDWR);
    if (mapping->fd == -1) {
        LOGE("Failed to open mapping file: %s, error: %s", mapping->file_path, strerror(errno));
        free(mapping->file_path);
        return false;
    }
    
    // 验证文件大小
    struct stat st;
    if (fstat(mapping->fd, &st) == -1) {
        LOGE("Failed to get file stats: %s", strerror(errno));
        close(mapping->fd);
        free(mapping->file_path);
        return false;
    }
    
    if ((size_t)st.st_size < size) {
        LOGE("File size mismatch: expected %zu, got %lld", size, (long long)st.st_size);
        close(mapping->fd);
        free(mapping->file_path);
        return false;
    }
    
    // 映射文件到内存
    mapping->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mapping->fd, 0);
    if (mapping->data == MAP_FAILED) {
        LOGE("Failed to map file to memory: %s", strerror(errno));
        close(mapping->fd);
        free(mapping->file_path);
        return false;
    }
    
    LOGI("Opened file mapping: %s, size: %zu", mapping->file_path, size);
    return true;
}

void sc_file_mapping_destroy(struct sc_file_mapping *mapping) {
    if (!mapping) {
        return;
    }

    if (mapping->data && mapping->data != MAP_FAILED) {
        munmap(mapping->data, mapping->size);
    }
    
    if (mapping->fd >= 0) {
        close(mapping->fd);
    }
    
    if (mapping->file_path) {
        // 只有创建者才删除文件
        if (mapping->is_creator) {
            unlink(mapping->file_path);
            LOGI("Removed mapping file: %s", mapping->file_path);
        }
        free(mapping->file_path);
    }
    
    memset(mapping, 0, sizeof(*mapping));
}

bool sc_file_mapping_write_frame(struct sc_file_mapping *mapping, 
                                 const struct sc_frame_header *header,
                                 const uint8_t *frame_data) {
    if (!mapping || !mapping->data || !header || !frame_data) {
        return false;
    }

    size_t total_size = sizeof(struct sc_frame_header) + header->frame_size;
    if (total_size > mapping->size) {
        LOGE("Frame too large for mapping buffer: %zu > %zu", total_size, mapping->size);
        return false;
    }

    struct sc_shared_frame_buffer *buffer = (struct sc_shared_frame_buffer *)mapping->data;
    
    // 写入头信息
    memcpy(&buffer->header, header, sizeof(struct sc_frame_header));
    
    // 写入帧数据
    memcpy(buffer->frame_data, frame_data, header->frame_size);
    
    // 确保数据写入磁盘
    msync(mapping->data, total_size, MS_ASYNC);
    
    return true;
}

bool sc_file_mapping_read_frame(struct sc_file_mapping *mapping,
                                struct sc_frame_header *header,
                                uint8_t *frame_data,
                                size_t max_size) {
    if (!mapping || !mapping->data || !header) {
        return false;
    }

    struct sc_shared_frame_buffer *buffer = (struct sc_shared_frame_buffer *)mapping->data;
    
    // 读取头信息
    memcpy(header, &buffer->header, sizeof(struct sc_frame_header));
    
    // 检查帧数据大小
    if (header->frame_size > max_size) {
        LOGE("Frame data too large for buffer: %u > %zu", header->frame_size, max_size);
        return false;
    }
    
    // 读取帧数据
    if (frame_data && header->frame_size > 0) {
        memcpy(frame_data, buffer->frame_data, header->frame_size);
    }
    
    return true;
}
