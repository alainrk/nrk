#ifndef nrk_debug_h
#define nrk_debug_h

#include "chunk.h"

int disassembleInstruction(Chunk *chunk, int offset);
void disassembleChunk(Chunk *chunk, const char *name);

#endif
