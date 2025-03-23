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
  __OP_STACK_RESET, // Reset the stack
  __OP_DUP,         // Internally used to duplicate the top of the stack
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_NOT_EQUAL,
  OP_GREATER_EQUAL,
  OP_LESS_EQUAL,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NEGATE,
  OP_PRINT,
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_GLOBAL,
  OP_GET_GLOBAL_LONG,
  OP_SET_GLOBAL,
  OP_SET_GLOBAL_LONG,
  OP_DEFINE_GLOBAL,
  OP_DEFINE_GLOBAL_LONG,
  OP_INCREMENT,
  OP_DECREMENT,
  OP_BITWISE_NOT,
  OP_BITWISE_AND,
  OP_BITWISE_OR,
  OP_BITWISE_SHIFT_RIGHT,
  OP_BITWISE_SHIFT_LEFT,
  OP_BITWISE_XOR,
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
