#ifndef SC_ENV_H
#define SC_ENV_H

#include "common.h"

// Return the value of the environment variable (may be NULL).
//
// The returned value must be freed by the caller.
char *
sc_get_env(const char *varname);

void sc_set_env(const char* varname, const char* variable, int is_overwrite);

#endif
