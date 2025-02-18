#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "value.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

VM *initVM() {
  VM *vm = (VM *)malloc(sizeof(VM));
  resetStack(vm);
  return vm;
}

void freeVM(VM *vm) {
  FREE_ARR(Value, vm->stack, vm->stackCap);
  free(vm);
};

void resetStack(VM *vm) {
  vm->stack = FREE_ARR(Value, vm->stack, vm->stackCap);

  // Let's grow the stack right away to avoid doing it everytime at runtime the
  // first time
  vm->stackCap = GROW_CAP(0);
  vm->stack = GROW_ARR(Value, vm->stack, 0, vm->stackCap);

  // Position the top of the stack at its beginning (first empty element)
  vm->stackTop = vm->stack;

  // #ifdef DEBUG_TRACE_EXECUTION
  //   printf("== stack resetted ==\n[ ");
  //   for (Value *v = vm->stack; v < vm->stackTop; v++) {
  //     printValue(*v, "", ", ");
  //   }
  //   printf("]\n=================\n");
  // #endif
}

void push(VM *vm, Value value) {
  if (vm->stackCap < (vm->stackTop - vm->stack + 1)) {
    int oldTop = vm->stackTop - vm->stack;
    vm->stackCap = GROW_CAP(vm->stackCap);
    vm->stack = GROW_ARR(Value, vm->stack, vm->stackCap, vm->stackCap);

    printf("\nGrowing stack: %d, %p\n", vm->stackCap, vm->stack);
    vm->stackTop = vm->stack + oldTop;
  }

  *vm->stackTop = value;
  vm->stackTop++;
}

Value pop(VM *vm) {
  vm->stackTop--;
  return *vm->stackTop;
}

// Better and faster way are writing ASM or using non standard C lib
// To keep things simple we use a switch statement
static InterpretResult run(VM *vm) {

#define READ_BYTE() (*vm->ip++)
#define MOVE_BYTES(n) (vm->ip += (n))
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG()                                                   \
  (vm->chunk->constants.values[GET_CONSTANT_LONG_ID(                           \
      vm->chunk, (int)(vm->ip - vm->chunk->code - 1))])
#define BINARY_OP(op)                                                          \
  do {                                                                         \
    double b = pop(vm);                                                        \
    double a = pop(vm);                                                        \
    push(vm, a op b);                                                          \
  } while (false)

  // Decode and dispatch loop
  for (;;) {

#ifdef DEBUG_TRACE_EXECUTION
    // Print out the stack
    printf("== stack ==\n[ ");
    for (Value *v = vm->stack; v < vm->stackTop; v++) {
      printValue(*v, "", ", ");
    }
    printf("]\n===========\n");
    // To get the offset we do some pointer math
    disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif

    u_int8_t instruction;
    switch (instruction = READ_BYTE()) {
    case __OP_STACK__RESET: {
      resetStack(vm);
      break;
    }
    case OP_NEGATE: {
      *(vm->stackTop - 1) = -*(vm->stackTop - 1);
      // Alternatively, pop and push
      // push(vm, -pop(vm));
      break;
    }
    case OP_ADD: {
      BINARY_OP(+);
      break;
    }
    case OP_SUBTRACT: {
      BINARY_OP(-);
      break;
    }
    case OP_MULTIPLY: {
      BINARY_OP(*);
      break;
    }
    case OP_DIVIDE: {
      BINARY_OP(/);
      break;
    }
    case OP_RETURN: {
      printValue(pop(vm), "Return: ", "\n");
      return INTERPRET_OK;
    }
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(vm, constant);
      break;
    }
    case OP_CONSTANT_LONG: {
      Value constant = READ_CONSTANT_LONG();
      push(vm, constant);
      MOVE_BYTES(3);
      printValue(constant, "---------\tRun OP_CONSTANT_LONG: ", "\n");
      break;
    }
    }
  }

#undef READ_BYTE
#undef MOVE_BYTES
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef BINARY_OP
}

// TODO: Remove after tests
InterpretResult interpretChunk(VM *vm, Chunk *chunk) {
  vm->chunk = chunk;
  vm->ip = vm->chunk->code;

  return run(vm);
}

InterpretResult interpret(VM *vm, const char *source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(vm, source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;

  InterpretResult res = run(vm);

  freeChunk(&chunk);
  return res;
}
