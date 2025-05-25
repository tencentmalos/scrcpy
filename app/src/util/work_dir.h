#pragma once

#ifdef __cplusplus
extern "C" {
#endif

const char* sc_query_work_directory();

int sc_set_work_directory(const char* work_dir);

const char* sc_combine_path(const char* work_dir, const char* path2);

#ifdef __cplusplus
}
#endif
