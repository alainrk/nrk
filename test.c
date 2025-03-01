#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "test.h"
#include "value.h"
#include "vm.h"

void testResetStack() {
  printf("\nRunning testResetStack()...\n");

  VM *vm = initVM();

  Chunk c;
  initChunk(&c);

  int numConst = 10;
  for (int i = 0; i < numConst; i++) {
    writeConstant(&c, NUMBER_VAL(666), 10);
  }

  writeChunk(&c, __OP_STACK__RESET, 11);
  writeConstant(&c, NUMBER_VAL(42), 12);

  // Return
  writeChunk(&c, OP_RETURN, 13);

  // Interpret
  interpretChunk(vm, &c);

  // Cleanup
  freeChunk(&c);

  freeVM(vm);
}

void testLongConst() {
  printf("\nRunning testLongConst()...\n");

  VM *vm = initVM();

  Chunk c;
  initChunk(&c);

  // Go beyond 255 to have the encoder using OP_CONSTANT_LONG
  int numConst = 285;
  for (int i = 0; i < numConst; i++) {

    uint8_t r = (uint8_t)rand();
    writeConstant(&c, NUMBER_VAL(r), 10);
  }

  // Negate
  writeConstant(&c, NUMBER_VAL(42), 1);
  writeChunk(&c, OP_NEGATE, 1);

  // Return
  writeChunk(&c, OP_RETURN, 11);

  // Interpret
  interpretChunk(vm, &c);

  // Cleanup
  freeChunk(&c);

  freeVM(vm);
}

void testAdd() {
  printf("\nRunning testAdd()...\n");

  VM *vm = initVM();

  Chunk c;
  initChunk(&c);

  writeConstant(&c, NUMBER_VAL(5.23), 10);
  writeConstant(&c, NUMBER_VAL(5.4), 10);

  writeChunk(&c, OP_ADD, 10);

  // Return
  writeChunk(&c, OP_RETURN, 11);

  // Interpret
  interpretChunk(vm, &c);

  // Cleanup
  freeChunk(&c);

  freeVM(vm);
}

void testArithmetics() {
  printf("\nRunning testArithmetics()...\n");

  VM *vm = initVM();

  Chunk c;
  initChunk(&c);

  writeConstant(&c, NUMBER_VAL(2), 10);
  writeConstant(&c, NUMBER_VAL(5), 10);
  writeChunk(&c, OP_ADD, 10);

  writeConstant(&c, NUMBER_VAL(3), 10);
  writeChunk(&c, OP_MULTIPLY, 10);

  writeConstant(&c, NUMBER_VAL(3), 10);
  writeChunk(&c, OP_DIVIDE, 10);

  // Return
  writeChunk(&c, OP_RETURN, 11);

  // Interpret
  interpretChunk(vm, &c);

  // Cleanup
  freeChunk(&c);

  freeVM(vm);
}

void testNegate() {
  printf("\nRunning testNegate()...\n");

  VM *vm = initVM();

  Chunk c;
  initChunk(&c);

  for (int i = 1; i < 998; i++) {
    writeConstant(&c, NUMBER_VAL(i), i);
    writeChunk(&c, OP_NEGATE, i);
  }

  writeChunk(&c, OP_RETURN, 1000);

  // Interpret
  interpretChunk(vm, &c);

  // Cleanup
  freeChunk(&c);

  freeVM(vm);
}
