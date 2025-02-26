#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION
// #define DEBUG_COMPILE_EXECUTION

// For intentionally unused parameters (e.g. ParseFn funcs have to have all the
// same signatures)
#define UNUSED(x) (void)(x)

char *stripstring(const char *input);

#endif
