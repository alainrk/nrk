#ifndef nrk_chunk_h
#define nrk_chunk_h

#include "common.h"
#include "line.h"
#include "value.h"

/*
 * Reads a 24-bit constant ID from a chunk's code array in a
 * platform-independent way. The bytes are read individually and combined using
 * bit shifts, ensuring consistent behavior regardless of the platform's
 * endianness.
 *
 * @param chunk  Pointer to the chunk structure containing the code array
 * @param offset Base offset into the code array where the constant ID starts
 * @return       24-bit constant ID value
 */
#define GET_CONSTANT_LONG_ID(chunk, offset)                                    \
  ((uint32_t)((chunk)->code[(offset) + 1] & 0xFF) << 16 |                      \
   (uint32_t)((chunk)->code[(offset) + 2] & 0xFF) << 8 |                       \
   (uint32_t)((chunk)->code[(offset) + 3] & 0xFF))

typedef enum {
  // Internal usage only operators
  __OP_STACK__RESET,
  // Available operators
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NEGATE,
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_NOT,
  OP_RETURN
} OpCode;

typedef struct {
  int count;
  int cap;
  uint8_t *code;
  LineArray lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);
void writeConstant(Chunk *chunk, Value value, int line);
int addConstant(Chunk *chunk, Value value);
int getConstantLong(Chunk *chunk, int offset);
int getInstructionLine(Chunk *chunk, int instrIdx);

#endif
