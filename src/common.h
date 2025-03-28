#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NRK_VERSION "0.0.1"

// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION
// #define DEBUG_SCAN_EXECUTION
// #define DEBUG_COMPILE_EXECUTION

#define DEBUG_COMPILE_INDENT_CHAR ' '

#define UINT8_COUNT (UINT8_MAX + 1)

// For intentionally unused parameters (e.g. ParseFn funcs have to have all the
// same signatures)
#define UNUSED(x) (void)(x)

char *stripstring(const char *input);
char *strfromnchars(const char c, size_t n);

#endif
