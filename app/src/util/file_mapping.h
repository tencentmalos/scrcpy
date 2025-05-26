#ifndef SC_FILE_MAPPING_H
#define SC_FILE_MAPPING_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// File mapping structure
struct sc_file_mapping {
    void *data;
    size_t size;
    char *file_path;
    int fd;
    bool is_creator;  // Whether this instance is the creator
};

// Image frame header information
#pragma pack(push, 1)
struct sc_frame_header {
    uint32_t width;
    uint32_t height;
    uint32_t format;        // Pixel format (YUV420P = 0, RGB24 = 1, RGBA = 2)
    uint32_t frame_size;    // Frame data size
    uint64_t timestamp;     // Timestamp
    uint32_t sequence;      // Sequence number
    uint32_t reserved0;   // Reserved field
    uint32_t reserved1;   // Reserved field
};
#pragma pack(pop)

// Shared frame buffer structure
struct sc_shared_frame_buffer {
    struct sc_frame_header header;
    uint8_t frame_data[];   // Variable-length frame data
};

// Create file mapping
bool sc_file_mapping_create(struct sc_file_mapping *mapping, const char *name, size_t size);

// Open existing file mapping
bool sc_file_mapping_open(struct sc_file_mapping *mapping, const char *name, size_t size);

// Destroy file mapping
void sc_file_mapping_destroy(struct sc_file_mapping *mapping);

// Write frame data to file mapping
bool sc_file_mapping_write_frame(struct sc_file_mapping *mapping, 
                                 const struct sc_frame_header *header,
                                 const uint8_t *frame_data);

// Read frame data from file mapping
bool sc_file_mapping_read_frame(struct sc_file_mapping *mapping,
                                struct sc_frame_header *header,
                                uint8_t *frame_data,
                                size_t max_size);

// Get temporary directory path
const char* sc_get_temp_dir(void);

#ifdef __cplusplus
}
#endif

#endif // SC_FILE_MAPPING_H
