#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "value.h"
#include <arpa/inet.h>
#include <stdarg.h>
#include <stddef.h>
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

// Report an error to the user and reset the stack as it is invalidated.
static void runtimeError(VM *vm, const char *format, ...) {
  va_list(args);
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  // We take the "previous" instruction as we've already advanced.
  size_t instruction = vm->ip - vm->chunk->code - 1;

  fprintf(stderr, "[Line %d] in script\n",
          getInstructionLine(vm->chunk, instruction));

  resetStack(vm);
}

// Returns the value on the stack at the given distance.
// Returns the top of the stack if dist is 0.
static Value peek(VM *vm, int dist) { return vm->stackTop[-1 - dist]; }

// Returns true is the value is nil, false or 0.
static bool isFalsey(Value v) {
  return IS_NIL(v) ||
         ((IS_BOOL(v) && !AS_BOOL(v)) || (IS_NUMBER(v) && AS_NUMBER(v) == 0));
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
// TODO: Handle strings binary operations
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {                  \
      runtimeError(vm, "Operands must be numbers.");                           \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop(vm));                                             \
    double a = AS_NUMBER(pop(vm));                                             \
    push(vm, valueType(a op b));                                               \
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
      if (!IS_NUMBER(peek(vm, 0))) {
        runtimeError(vm, "Operand must be a number");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm->stackTop[-1].as.number = -peek(vm, 0).as.number;
      break;
    }
    case OP_ADD: {
      BINARY_OP(NUMBER_VAL, +);
      break;
    }
    case OP_SUBTRACT: {
      BINARY_OP(NUMBER_VAL, -);
      break;
    }
    case OP_MULTIPLY: {
      BINARY_OP(NUMBER_VAL, *);
      break;
    }
    case OP_DIVIDE: {
      BINARY_OP(NUMBER_VAL, /);
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
    case OP_NIL: {
      push(vm, NIL_VAL);
      break;
    }
    case OP_TRUE: {
      push(vm, BOOL_VAL(true));
      break;
    }
    case OP_FALSE: {
      push(vm, BOOL_VAL(false));
      break;
    }
    case OP_NOT: {
      vm->stackTop[-1] = BOOL_VAL(isFalsey(peek(vm, 0)));
      break;
    }
    case OP_EQUAL: {
      Value a = pop(vm);
      Value b = pop(vm);
      push(vm, BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_GREATER: {
      BINARY_OP(BOOL_VAL, >);
      break;
    }
    case OP_LESS: {
      BINARY_OP(BOOL_VAL, <);
      break;
    }
    case OP_LESS_EQUAL: {
      BINARY_OP(BOOL_VAL, <=);
      break;
    }
    case OP_GREATER_EQUAL: {
      BINARY_OP(BOOL_VAL, >=);
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

InterpretResult interpretChunk(VM *vm, Chunk *chunk) {
  vm->chunk = chunk;
  vm->ip = vm->chunk->code;

  return run(vm);
}

InterpretResult interpret(VM *vm, const char *source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;

  InterpretResult res = run(vm);

  freeChunk(&chunk);
  return res;
}
