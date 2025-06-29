#include "debug.h"
#include "chunk.h"
#include <stdint.h>
#include <stdio.h>

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int jumpInstruction(const char *name, int sign, Chunk *chunk,
                           int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

static int byteInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];

  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant], "", "'\n");

  // 1 byte opcode + 1 byte operand
  return offset + 2;
}

static int constantLongInstruction(const char *name, Chunk *chunk, int offset) {
  uint32_t constant = GET_CONSTANT_LONG_ID(chunk, offset);

  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant], "", "'\n");

  // 1 byte opcode + 3 bytes operand
  return offset + 4;
}

int disassembleInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);

  int prevLine = getInstructionLine(chunk, offset - 1);
  int currLine = getInstructionLine(chunk, offset);

  if (offset > 0 && prevLine == currLine) {
    printf("         | ");
  } else {
    printf("line: %4d ", currLine);
  }

  u_int8_t instr = chunk->code[offset];
  switch (instr) {
  case __OP_STACK_RESET:
    return simpleInstruction("__OP_STACK_RESET", offset);
  case __OP_DUP:
    return simpleInstruction("__OP_DUP", offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);
  case OP_PRINT:
    return simpleInstruction("OP_PRINT", offset);
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simpleInstruction("OP_DIVIDE", offset);
  case OP_NIL:
    return simpleInstruction("OP_NIL", offset);
  case OP_TRUE:
    return simpleInstruction("OP_TRUE", offset);
  case OP_FALSE:
    return simpleInstruction("OP_FALSE", offset);
  case OP_NOT:
    return simpleInstruction("OP_NOT", offset);
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_NOT_EQUAL:
    return simpleInstruction("OP_NOT_EQUAL", offset);
  case OP_GREATER_EQUAL:
    return simpleInstruction("OP_GREATER_EQUAL", offset);
  case OP_LESS_EQUAL:
    return simpleInstruction("OP_LESS_EQUAL", offset);
  case OP_POP:
    return simpleInstruction("OP_POP", offset);
  case OP_INCREMENT:
    return simpleInstruction("OP_INCREMENT", offset);
  case OP_DECREMENT:
    return simpleInstruction("OP_DECREMENT", offset);
  case OP_BITWISE_SHIFT_RIGHT:
    return simpleInstruction("OP_BITWISE_SHIFT_RIGHT", offset);
  case OP_BITWISE_SHIFT_LEFT:
    return simpleInstruction("OP_BITWISE_SHIFT_LEFT", offset);
  case OP_BITWISE_NOT:
    return simpleInstruction("OP_BITWISE_NOT", offset);
  case OP_BITWISE_AND:
    return simpleInstruction("OP_BITWISE_AND", offset);
  case OP_BITWISE_OR:
    return simpleInstruction("OP_BITWISE_OR", offset);
  case OP_BITWISE_XOR:
    return simpleInstruction("OP_BITWISE_XOR", offset);
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_CONSTANT_LONG:
    return constantLongInstruction("OP_CONSTANT_LONG", chunk, offset);
  case OP_GET_GLOBAL:
    return constantInstruction("OP_GET_GLOBAL", chunk, offset);
  case OP_GET_GLOBAL_LONG:
    return constantLongInstruction("OP_GET_GLOBAL_LONG", chunk, offset);
  case OP_SET_GLOBAL:
    return constantInstruction("OP_SET_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL_LONG:
    return constantLongInstruction("OP_SET_GLOBAL_LONG", chunk, offset);
  case OP_DEFINE_GLOBAL:
    return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_DEFINE_GLOBAL_LONG:
    return constantLongInstruction("OP_DEFINE_GLOBAL_LONG", chunk, offset);
  case OP_GET_LOCAL:
    return byteInstruction("OP_GET_LOCAL", chunk, offset);
  case OP_SET_LOCAL:
    return byteInstruction("OP_SET_LOCAL", chunk, offset);
  case OP_JUMP:
    return jumpInstruction("OP_JUMP", 1, chunk, offset);
  case OP_JUMP_IF_FALSE:
    return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
  default:
    printf("Unknown opcode %d\n", instr);
    return offset + 1;
  }
}

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  // Let disassembleInstruction increment the offset as instructions have
  // different sizes.
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}
