#include "file_mapping.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h> // For strerror, used by both

#ifdef _WIN32
#include <windows.h>
#else // POSIX
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#include "util/log.h"

// Helper function for Windows error messages
#ifdef _WIN32
static char* sc_win_error_to_string(DWORD error_code) {
    LPSTR message_buffer = NULL;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message_buffer, 0, NULL);
    if (size == 0) {
        return strdup("Unknown error");
    }
    char* message = strdup(message_buffer);
    LocalFree(message_buffer);
    return message;
}
#endif

const char* sc_get_temp_dir(void) {
    const char* temp_dir = getenv("TMP"); // Windows typically uses TMP or TEMP
    if (temp_dir && strlen(temp_dir) > 0) {
        return temp_dir;
    }
    
    temp_dir = getenv("TEMP");
    if (temp_dir && strlen(temp_dir) > 0) {
        return temp_dir;
    }

#ifndef _WIN32 // POSIX specific
    temp_dir = getenv("TMPDIR");
    if (temp_dir && strlen(temp_dir) > 0) {
        return temp_dir;
    }
    return "/tmp"; // Default for POSIX if others are not set
#else
    // Windows default if TMP/TEMP not set (less common but possible)
    // GetWindowsDirectory or GetTempPath could be used for a more robust default
    static char windows_temp_path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, windows_temp_path) > 0) {
        // Remove trailing backslash if present for consistency
        size_t len = strlen(windows_temp_path);
        if (len > 0 && (windows_temp_path[len - 1] == '\\' || windows_temp_path[len - 1] == '/')) {
            windows_temp_path[len - 1] = '\0';
        }
        return windows_temp_path;
    }
    return "C:\\Temp"; // Fallback, though GetTempPath should usually work
#endif
}

#ifdef _WIN32
bool sc_file_mapping_create(struct sc_file_mapping *mapping, const char *name, size_t size) {
    if (!mapping || !name || size == 0) {
        return false;
    }

    memset(mapping, 0, sizeof(*mapping));
    mapping->fd = INVALID_HANDLE_VALUE;
    mapping->hMapFile = NULL;
    mapping->size = size;
    mapping->is_creator = true;

    const char* temp_dir = sc_get_temp_dir();
    size_t path_len = strlen(temp_dir) + 1 + strlen(name) + 20; // +1 for path separator
    mapping->file_path = malloc(path_len);
    if (!mapping->file_path) {
        return false;
    }
    snprintf(mapping->file_path, path_len, "%s\\scrcpy_%s.map", temp_dir, name); // Use backslash for Windows

    mapping->fd = CreateFileA(
        mapping->file_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, // Allow sharing for open
        NULL,
        CREATE_ALWAYS, // Creates a new file, always. Overwrites if exists.
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (mapping->fd == INVALID_HANDLE_VALUE) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to create mapping file: %s, error: %s", mapping->file_path, err_msg);
        free(err_msg);
        free(mapping->file_path);
        return false;
    }

    // Set file size
    LARGE_INTEGER li_size;
    li_size.QuadPart = size;
    if (!SetFilePointerEx(mapping->fd, li_size, NULL, FILE_BEGIN) || !SetEndOfFile(mapping->fd)) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to set file size for %s, error: %s", mapping->file_path, err_msg);
        free(err_msg);
        CloseHandle(mapping->fd);
        DeleteFileA(mapping->file_path);
        free(mapping->file_path);
        return false;
    }
    
    char mappedName[128];
    snprintf(mappedName, sizeof(mappedName), "scrcpy_%s.map", name);

    mapping->hMapFile = CreateFileMappingA(
        mapping->fd,
        NULL,
        PAGE_READWRITE,
        0, // High-order DWORD of the maximum size of the file mapping object.
        (DWORD)size, // Low-order DWORD of the maximum size.
        mappedName); // Name of the mapping object (NULL for unnamed)

    if (mapping->hMapFile == NULL) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to create file mapping object for %s, error: %s", mapping->file_path, err_msg);
        free(err_msg);
        CloseHandle(mapping->fd);
        DeleteFileA(mapping->file_path);
        free(mapping->file_path);
        return false;
    }

    mapping->data = MapViewOfFile(
        mapping->hMapFile,
        FILE_MAP_ALL_ACCESS, // Read/write permission
        0, // High-order DWORD of the file offset where the view begins.
        0, // Low-order DWORD of the file offset.
        size); // Number of bytes of a file mapping to map to the view.

    if (mapping->data == NULL) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to map view of file for %s, error: %s", mapping->file_path, err_msg);
        free(err_msg);
        CloseHandle(mapping->hMapFile);
        CloseHandle(mapping->fd);
        DeleteFileA(mapping->file_path);
        free(mapping->file_path);
        return false;
    }

    memset(mapping->data, 0, size);
    LOGI("Created file mapping (Win): %s, size: %zu, name:%s", mapping->file_path, size, mappedName);
    return true;
}

bool sc_file_mapping_open(struct sc_file_mapping *mapping, const char *name, size_t size) {
    if (!mapping || !name || size == 0) {
        return false;
    }
    
    memset(mapping, 0, sizeof(*mapping));
    mapping->fd = INVALID_HANDLE_VALUE;
    mapping->hMapFile = NULL;
    mapping->size = size;
    mapping->is_creator = false;

    const char* temp_dir = sc_get_temp_dir();
    size_t path_len = strlen(temp_dir) + 1 + strlen(name) + 20;
    mapping->file_path = malloc(path_len);
    if (!mapping->file_path) {
        return false;
    }
    snprintf(mapping->file_path, path_len, "%s\\scrcpy_%s.map", temp_dir, name);

    mapping->fd = CreateFileA(
        mapping->file_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (mapping->fd == INVALID_HANDLE_VALUE) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to open mapping file (Win): %s, error: %s", mapping->file_path, err_msg);
        free(err_msg);
        free(mapping->file_path);
        return false;
    }

    LARGE_INTEGER file_size_li;
    if (!GetFileSizeEx(mapping->fd, &file_size_li)) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to get file size for %s, error: %s", mapping->file_path, err_msg);
        free(err_msg);
        CloseHandle(mapping->fd);
        free(mapping->file_path);
        return false;
    }

    if ((size_t)file_size_li.QuadPart < size) {
        LOGE("File size mismatch (Win): expected %zu, got %lld", size, (long long)file_size_li.QuadPart);
        CloseHandle(mapping->fd);
        free(mapping->file_path);
        return false;
    }
    
    mapping->hMapFile = CreateFileMappingA(
        mapping->fd,
        NULL,
        PAGE_READWRITE,
        0, (DWORD)size, // Use the requested size for mapping, not necessarily entire file
        NULL); 

    if (mapping->hMapFile == NULL) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to create file mapping object for opening %s, error: %s", mapping->file_path, err_msg);
        free(err_msg);
        CloseHandle(mapping->fd);
        free(mapping->file_path);
        return false;
    }

    mapping->data = MapViewOfFile(
        mapping->hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0, size);

    if (mapping->data == NULL) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to map view of file for opening %s, error: %s", mapping->file_path, err_msg);
        free(err_msg);
        CloseHandle(mapping->hMapFile);
        CloseHandle(mapping->fd);
        free(mapping->file_path);
        return false;
    }
    
    LOGI("Opened file mapping (Win): %s, size: %zu", mapping->file_path, size);
    return true;
}

void sc_file_mapping_destroy(struct sc_file_mapping *mapping) {
    if (!mapping) {
        return;
    }

    if (mapping->data) {
        UnmapViewOfFile(mapping->data);
    }
    if (mapping->hMapFile) {
        CloseHandle(mapping->hMapFile);
    }
    if (mapping->fd != INVALID_HANDLE_VALUE) {
        CloseHandle(mapping->fd);
    }
    
    if (mapping->file_path) {
        if (mapping->is_creator) {
            if (DeleteFileA(mapping->file_path)) {
                 LOGI("Removed mapping file (Win): %s", mapping->file_path);
            } else {
                 char* err_msg = sc_win_error_to_string(GetLastError());
                 LOGE("Failed to remove mapping file (Win): %s, error: %s", mapping->file_path, err_msg);
                 free(err_msg);
            }
        }
        free(mapping->file_path);
    }
    memset(mapping, 0, sizeof(*mapping));
    mapping->fd = INVALID_HANDLE_VALUE; // Reset after memset
}

#else // POSIX implementation

bool sc_file_mapping_create(struct sc_file_mapping *mapping, const char *name, size_t size) {
    if (!mapping || !name || size == 0) {
        return false;
    }

    memset(mapping, 0, sizeof(*mapping));
    mapping->fd = -1; // Initialize fd for POSIX
    mapping->size = size;
    mapping->is_creator = true;
    
    const char* temp_dir = sc_get_temp_dir();
    size_t path_len = strlen(temp_dir) + 1 + strlen(name) + 20; // +1 for path separator
    mapping->file_path = malloc(path_len);
    if (!mapping->file_path) {
        return false;
    }
    // Use / for POSIX
    snprintf(mapping->file_path, path_len, "%s/scrcpy_%s.map", temp_dir, name);
    
    mapping->fd = open(mapping->file_path, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (mapping->fd == -1) {
        LOGE("Failed to create mapping file: %s, error: %s", mapping->file_path, strerror(errno));
        free(mapping->file_path);
        return false;
    }
    
    if (ftruncate(mapping->fd, size) == -1) {
        LOGE("Failed to set file size: %s", strerror(errno));
        close(mapping->fd);
        unlink(mapping->file_path);
        free(mapping->file_path);
        return false;
    }
    
    mapping->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mapping->fd, 0);
    if (mapping->data == MAP_FAILED) {
        LOGE("Failed to map file to memory: %s", strerror(errno));
        close(mapping->fd);
        unlink(mapping->file_path);
        free(mapping->file_path);
        return false;
    }
    
    memset(mapping->data, 0, size);
    LOGI("Created file mapping (POSIX): %s, size: %zu", mapping->file_path, size);
    return true;
}

bool sc_file_mapping_open(struct sc_file_mapping *mapping, const char *name, size_t size) {
    if (!mapping || !name || size == 0) {
        return false;
    }

    memset(mapping, 0, sizeof(*mapping));
    mapping->fd = -1; // Initialize fd for POSIX
    mapping->size = size;
    mapping->is_creator = false;
    
    const char* temp_dir = sc_get_temp_dir();
    size_t path_len = strlen(temp_dir) + 1 + strlen(name) + 20;
    mapping->file_path = malloc(path_len);
    if (!mapping->file_path) {
        return false;
    }
    snprintf(mapping->file_path, path_len, "%s/scrcpy_%s.map", temp_dir, name);
    
    mapping->fd = open(mapping->file_path, O_RDWR);
    if (mapping->fd == -1) {
        LOGE("Failed to open mapping file (POSIX): %s, error: %s", mapping->file_path, strerror(errno));
        free(mapping->file_path);
        return false;
    }
    
    struct stat st;
    if (fstat(mapping->fd, &st) == -1) {
        LOGE("Failed to get file stats (POSIX): %s", strerror(errno));
        close(mapping->fd);
        free(mapping->file_path);
        return false;
    }
    
    if ((size_t)st.st_size < size) {
        LOGE("File size mismatch (POSIX): expected %zu, got %lld", size, (long long)st.st_size);
        close(mapping->fd);
        free(mapping->file_path);
        return false;
    }
    
    mapping->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mapping->fd, 0);
    if (mapping->data == MAP_FAILED) {
        LOGE("Failed to map file to memory (POSIX): %s", strerror(errno));
        close(mapping->fd);
        free(mapping->file_path);
        return false;
    }
    
    LOGI("Opened file mapping (POSIX): %s, size: %zu", mapping->file_path, size);
    return true;
}

void sc_file_mapping_destroy(struct sc_file_mapping *mapping) {
    if (!mapping) {
        return;
    }

    if (mapping->data && mapping->data != MAP_FAILED) { // MAP_FAILED is ((void *) -1)
        munmap(mapping->data, mapping->size);
    }
    
    if (mapping->fd >= 0) { // For POSIX, fd is non-negative on success
        close(mapping->fd);
    }
    
    if (mapping->file_path) {
        if (mapping->is_creator) {
            if (unlink(mapping->file_path) == 0) {
                LOGI("Removed mapping file (POSIX): %s", mapping->file_path);
            } else {
                LOGE("Failed to remove mapping file (POSIX): %s, error: %s", mapping->file_path, strerror(errno));
            }
        }
        free(mapping->file_path);
    }
    memset(mapping, 0, sizeof(*mapping));
    mapping->fd = -1; // Reset after memset for POSIX
}
#endif // _WIN32 vs POSIX

bool sc_file_mapping_write_frame(struct sc_file_mapping *mapping, 
                                 const struct sc_frame_header *header,
                                 const uint8_t *frame_data) {
    if (!mapping || !mapping->data || !header || !frame_data) {
        return false;
    }
#ifdef _WIN32
    if (mapping->data == NULL) return false; // Check for Win mapping
#else
    if (mapping->data == MAP_FAILED) return false; // Check for POSIX mapping
#endif

    size_t total_size = sizeof(struct sc_frame_header) + header->frame_size;
    if (total_size > mapping->size) {
        LOGE("Frame too large for mapping buffer: %zu > %zu", total_size, mapping->size);
        return false;
    }

    struct sc_shared_frame_buffer *buffer = (struct sc_shared_frame_buffer *)mapping->data;
    
    memcpy(&buffer->header, header, sizeof(struct sc_frame_header));
    memcpy(buffer->frame_data, frame_data, header->frame_size);
    
#ifdef _WIN32
    if (!FlushViewOfFile(mapping->data, total_size)) {
        char* err_msg = sc_win_error_to_string(GetLastError());
        LOGE("Failed to flush view of file (Win): %s", err_msg);
        free(err_msg);
        // Not necessarily fatal for the write itself, but data might not be on disk
    }
#else
    if (msync(mapping->data, total_size, MS_ASYNC) == -1) {
        LOGE("Failed to msync (POSIX): %s", strerror(errno));
        // Not necessarily fatal
    }
#endif
    
    return true;
}

bool sc_file_mapping_read_frame(struct sc_file_mapping *mapping,
                                struct sc_frame_header *header,
                                uint8_t *frame_data,
                                size_t max_size) {
    if (!mapping || !mapping->data || !header) {
        return false;
    }
#ifdef _WIN32
    if (mapping->data == NULL) return false;
#else
    if (mapping->data == MAP_FAILED) return false;
#endif

    struct sc_shared_frame_buffer *buffer = (struct sc_shared_frame_buffer *)mapping->data;
    
    memcpy(header, &buffer->header, sizeof(struct sc_frame_header));
    
    if (header->frame_size > max_size) {
        LOGE("Frame data too large for buffer: %u > %zu", header->frame_size, max_size);
        return false;
    }
    
    if (frame_data && header->frame_size > 0) {
        memcpy(frame_data, buffer->frame_data, header->frame_size);
    }
    
    return true;
}
