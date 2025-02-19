#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "line.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->cap = 0;
  chunk->code = NULL;
  initValueArray(&chunk->constants);
  initLineArray(&chunk->lines);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
  if (chunk->cap < chunk->count + 1) {
    int oldCap = chunk->cap;
    chunk->cap = GROW_CAP(oldCap);
    chunk->code = GROW_ARR(uint8_t, chunk->code, oldCap, chunk->cap);
  }

  setLine(&chunk->lines, line);

  chunk->code[chunk->count] = byte;
  chunk->count++;
}

void writeConstant(Chunk *chunk, Value value, int line) {
  int idx = addConstant(chunk, value);

  if (idx < 256) {
    // Use OP_CONSTANT
    writeChunk(chunk, OP_CONSTANT, line);
    writeChunk(chunk, idx, line);
    return;
  }

  // Use OP_CONSTANT_LONG
  writeChunk(chunk, OP_CONSTANT_LONG, line);

  // Write the 24-bit (3 bytes)
  // Apply AND bit by bit for the relevant part and get rid of the rest
  writeChunk(chunk, (idx & 0xff0000) >> 16, line);
  writeChunk(chunk, (idx & 0x00ff00) >> 8, line);
  writeChunk(chunk, (idx & 0x0000ff), line);
}

int getInstructionLine(Chunk *chunk, int instrIdx) {
  int line = getLine(&chunk->lines, instrIdx);
  return line;
}

// Returns the index of the inserted element.
int addConstant(Chunk *chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}

void freeChunk(Chunk *chunk) {
  FREE_ARR(uint8_t, chunk->code, chunk->cap);
  freeValueArray(&chunk->constants);
  freeLineArray(&chunk->lines);
  initChunk(chunk);
}
