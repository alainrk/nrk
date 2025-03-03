#ifndef nrk_vm_h
#define nrk_vm_h

#include "chunk.h"
#include "value.h"

typedef struct {
  // Chunk to be executed
  Chunk *chunk;

  // Instruction Pointer about to be executed (first byte of chunk.code)
  // It is a real pointer as it's easier and faster to use memory directly
  // Note: it is called PC (Program Counter in some arch like ARM)
  uint8_t *ip;

  // Dynamically growing stack
  int stackCap;
  Value *stack;

  // Pointer to the first empty item (allowed in C) i.e. the next one to fill
  Value *stackTop;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

VM *initVM();
void freeVM(VM *vm);
InterpretResult interpretChunk(VM *vm, Chunk *chunk);
InterpretResult interpret(VM *vm, const char *source);
void resetStack(VM *vm);
void push(VM *vm, Value value);
Value pop(VM *vm);
bool valuesEqual(Value a, Value b);

#endif
