#include "work_dir.h"
#include <iostream>
#include <unistd.h> // For chdir() and getcwd()
#include <cerrno>   // For errno
#include <cstring>  // For strerror()
#include <limits.h> // For PATH_MAX (or use a sufficiently large buffer)

const char* sc_query_work_directory() {
    char cwd_buffer[PATH_MAX] = { 0 };
    if (getcwd(cwd_buffer, sizeof(cwd_buffer)) != nullptr) {
        return strdup(cwd_buffer);
    } else {
        return "";
    }
}

int sc_set_work_directory(const char* work_dir) {
    return chdir(work_dir);
}

const char* sc_combine_path(const char* work_dir, const char* path2) {
    char cwd_buffer[PATH_MAX] = { 0 };
    snprintf(cwd_buffer, PATH_MAX, "%s/%s", work_dir, path2);
    return strdup(cwd_buffer);
}
