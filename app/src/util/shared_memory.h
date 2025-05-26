#ifndef SC_SHARED_MEMORY_H
#define SC_SHARED_MEMORY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Shared memory structure
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

// Image frame header information
struct sc_frame_header {
    uint32_t width;
    uint32_t height;
    uint32_t format;        // Pixel format (YUV420P = 0, RGB24 = 1, RGBA = 2)
    uint32_t frame_size;    // Frame data size
    uint64_t timestamp;     // Timestamp
    uint32_t sequence;      // Sequence number
    uint32_t reserved[2];   // Reserved fields
};

// Shared memory buffer structure
struct sc_shared_frame_buffer {
    struct sc_frame_header header;
    uint8_t frame_data[];   // Variable-length frame data
};

// Create shared memory
bool sc_shared_memory_create(struct sc_shared_memory *shm, const char *name, size_t size);

// Open existing shared memory
bool sc_shared_memory_open(struct sc_shared_memory *shm, const char *name, size_t size);

// Destroy shared memory
void sc_shared_memory_destroy(struct sc_shared_memory *shm);

// Write frame data to shared memory
bool sc_shared_memory_write_frame(struct sc_shared_memory *shm, 
                                  const struct sc_frame_header *header,
                                  const uint8_t *frame_data);

// Read frame data from shared memory
bool sc_shared_memory_read_frame(struct sc_shared_memory *shm,
                                 struct sc_frame_header *header,
                                 uint8_t *frame_data,
                                 size_t max_size);

#ifdef __cplusplus
}
#endif

#endif // SC_SHARED_MEMORY_H
