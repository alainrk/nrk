#ifndef nrk_compiler_h
#define nrk_compiler_h

#include "vm.h"

bool compile(VM *vm, const char *source, Chunk *chunk);

#endif
