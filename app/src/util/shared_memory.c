#include "shared_memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __APPLE__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "../util/log.h"

bool sc_shared_memory_create(struct sc_shared_memory *shm, const char *name, size_t size) {
    if (!shm || !name || size == 0) {
        return false;
    }

    memset(shm, 0, sizeof(*shm));
    shm->size = size;
    
    // Copy name
    shm->name = strdup(name);
    if (!shm->name) {
        return false;
    }

#ifdef __APPLE__
    // macOS uses POSIX shared memory
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/%s", name);
    
    // Create shared memory object
    shm->fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm->fd == -1) {
        LOGE("Failed to create shared memory: %s", shm_name);
        free(shm->name);
        return false;
    }
    
    // Set size
    if (ftruncate(shm->fd, size) == -1) {
        LOGE("Failed to set shared memory size");
        close(shm->fd);
        shm_unlink(shm_name);
        free(shm->name);
        return false;
    }
    
    // Map memory
    shm->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) {
        LOGE("Failed to map shared memory");
        close(shm->fd);
        shm_unlink(shm_name);
        free(shm->name);
        return false;
    }

#elif defined(_WIN32)
    // Windows uses file mapping
    wchar_t wide_name[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, sizeof(wide_name) / sizeof(wchar_t));
    
    shm->handle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 
                                     (DWORD)(size >> 32), (DWORD)size, wide_name);
    if (!shm->handle) {
        LOGE("Failed to create file mapping");
        free(shm->name);
        return false;
    }
    
    shm->data = MapViewOfFile(shm->handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!shm->data) {
        LOGE("Failed to map view of file");
        CloseHandle(shm->handle);
        free(shm->name);
        return false;
    }

#else
    // Linux uses POSIX shared memory
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/%s", name);
    
    shm->fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm->fd == -1) {
        LOGE("Failed to create shared memory: %s", shm_name);
        free(shm->name);
        return false;
    }
    
    if (ftruncate(shm->fd, size) == -1) {
        LOGE("Failed to set shared memory size");
        close(shm->fd);
        shm_unlink(shm_name);
        free(shm->name);
        return false;
    }
    
    shm->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) {
        LOGE("Failed to map shared memory");
        close(shm->fd);
        shm_unlink(shm_name);
        free(shm->name);
        return false;
    }
#endif

    LOGI("Created shared memory: %s, size: %zu", name, size);
    return true;
}

bool sc_shared_memory_open(struct sc_shared_memory *shm, const char *name, size_t size) {
    if (!shm || !name || size == 0) {
        return false;
    }

    memset(shm, 0, sizeof(*shm));
    shm->size = size;
    
    shm->name = strdup(name);
    if (!shm->name) {
        return false;
    }

#ifdef __APPLE__
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/%s", name);
    
    shm->fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm->fd == -1) {
        LOGE("Failed to open shared memory: %s", shm_name);
        free(shm->name);
        return false;
    }
    
    shm->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) {
        LOGE("Failed to map shared memory");
        close(shm->fd);
        free(shm->name);
        return false;
    }

#elif defined(_WIN32)
    wchar_t wide_name[256];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, sizeof(wide_name) / sizeof(wchar_t));
    
    shm->handle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, wide_name);
    if (!shm->handle) {
        LOGE("Failed to open file mapping");
        free(shm->name);
        return false;
    }
    
    shm->data = MapViewOfFile(shm->handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!shm->data) {
        LOGE("Failed to map view of file");
        CloseHandle(shm->handle);
        free(shm->name);
        return false;
    }

#else
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/%s", name);
    
    shm->fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm->fd == -1) {
        LOGE("Failed to open shared memory: %s", shm_name);
        free(shm->name);
        return false;
    }
    
    shm->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED) {
        LOGE("Failed to map shared memory");
        close(shm->fd);
        free(shm->name);
        return false;
    }
#endif

    LOGI("Opened shared memory: %s, size: %zu", name, size);
    return true;
}

void sc_shared_memory_destroy(struct sc_shared_memory *shm) {
    if (!shm) {
        return;
    }

#ifdef __APPLE__
    if (shm->data && shm->data != MAP_FAILED) {
        munmap(shm->data, shm->size);
    }
    if (shm->fd >= 0) {
        close(shm->fd);
        if (shm->name) {
            char shm_name[256];
            snprintf(shm_name, sizeof(shm_name), "/%s", shm->name);
            shm_unlink(shm_name);
        }
    }

#elif defined(_WIN32)
    if (shm->data) {
        UnmapViewOfFile(shm->data);
    }
    if (shm->handle) {
        CloseHandle(shm->handle);
    }

#else
    if (shm->data && shm->data != MAP_FAILED) {
        munmap(shm->data, shm->size);
    }
    if (shm->fd >= 0) {
        close(shm->fd);
        if (shm->name) {
            char shm_name[256];
            snprintf(shm_name, sizeof(shm_name), "/%s", shm->name);
            shm_unlink(shm_name);
        }
    }
#endif

    if (shm->name) {
        free(shm->name);
    }
    
    memset(shm, 0, sizeof(*shm));
}

bool sc_shared_memory_write_frame(struct sc_shared_memory *shm, 
                                  const struct sc_frame_header *header,
                                  const uint8_t *frame_data) {
    if (!shm || !shm->data || !header || !frame_data) {
        return false;
    }

    size_t total_size = sizeof(struct sc_frame_header) + header->frame_size;
    if (total_size > shm->size) {
        LOGE("Frame too large for shared memory buffer");
        return false;
    }

    struct sc_shared_frame_buffer *buffer = (struct sc_shared_frame_buffer *)shm->data;
    
    // Write header information
    memcpy(&buffer->header, header, sizeof(struct sc_frame_header));
    
    // Write frame data
    memcpy(buffer->frame_data, frame_data, header->frame_size);
    
    return true;
}

bool sc_shared_memory_read_frame(struct sc_shared_memory *shm,
                                 struct sc_frame_header *header,
                                 uint8_t *frame_data,
                                 size_t max_size) {
    if (!shm || !shm->data || !header) {
        return false;
    }

    struct sc_shared_frame_buffer *buffer = (struct sc_shared_frame_buffer *)shm->data;
    
    // Read header information
    memcpy(header, &buffer->header, sizeof(struct sc_frame_header));
    
    // Check frame data size
    if (header->frame_size > max_size) {
        LOGE("Frame data too large for buffer");
        return false;
    }
    
    // Read frame data
    if (frame_data) {
        memcpy(frame_data, buffer->frame_data, header->frame_size);
    }
    
    return true;
}
