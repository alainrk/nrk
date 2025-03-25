#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include <arpa/inet.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VM *initVM() {
  VM *vm = (VM *)malloc(sizeof(VM));
  vm->memoryManager = initMemoryManager();
  vm->compiler = initCompiler(vm->memoryManager);
  resetStack(vm);
  return vm;
}

void freeVM(VM *vm) {
  FREE_ARR(Value, vm->stack, vm->stackCap);

  freeCompiler(vm->compiler);
  vm->compiler = NULL;

  freeMemoryManager(vm->memoryManager);
  vm->memoryManager = NULL;

  free(vm);
};

void resetStack(VM *vm) {
  if (vm->stack != NULL) {
    vm->stack = FREE_ARR(Value, vm->stack, vm->stackCap);
  }

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

static void concatenate(VM *vm) {
  // The order must be [ b, a ] to preserve the stack fifo sort.
  ObjString *b = AS_STRING(pop(vm));
  ObjString *a = AS_STRING(pop(vm));

  int len = a->length + b->length;
  char *str = ALLOCATE(char, len + 1);

  memcpy(str, a->str, a->length);
  memcpy(str + a->length, b->str, b->length);
  str[len] = '\0';

  ObjString *c = takeString(vm->memoryManager, str, len);
  push(vm, OBJ_VAL(c));
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

#define READ_STRING() AS_STRING(READ_CONSTANT())

#define READ_STRING_LONG() AS_STRING(READ_CONSTANT_LONG())

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

#define BINARY_OP_BITWISE(op)                                                  \
  do {                                                                         \
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {                  \
      runtimeError(vm, "Bitwise operands must be numbers.");                   \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    int64_t b = (int64_t)AS_NUMBER(pop(vm));                                   \
    int64_t a = (int64_t)AS_NUMBER(pop(vm));                                   \
    push(vm, NUMBER_VAL(a op b));                                              \
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
    case __OP_STACK_RESET: {
      resetStack(vm);
      break;
    }
    case __OP_DUP: {
      push(vm, peek(vm, 0));
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
      if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
        concatenate(vm);
      } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
        push(vm, NUMBER_VAL(AS_NUMBER(pop(vm)) + AS_NUMBER(pop(vm))));
      } else {
        runtimeError(vm, "Operands must be both either strings or numbers");
        return INTERPRET_RUNTIME_ERROR;
      }

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
    case OP_BITWISE_NOT: {
      if (!IS_NUMBER(peek(vm, 0))) {
        runtimeError(vm, "Cannot apply bitwise not on non numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }

      int64_t result = ~(int64_t)AS_NUMBER(peek(vm, 0));
      vm->stackTop[-1].as.number = (double)result;

      break;
    }
    case OP_BITWISE_SHIFT_RIGHT: {
      BINARY_OP_BITWISE(>>);
      break;
    }
    case OP_BITWISE_SHIFT_LEFT: {
      BINARY_OP_BITWISE(<<);
      break;
    }
    case OP_BITWISE_AND: {
      BINARY_OP_BITWISE(&);
      break;
    }
    case OP_BITWISE_OR: {
      BINARY_OP_BITWISE(|);
      break;
    }
    case OP_BITWISE_XOR: {
      BINARY_OP_BITWISE(^);
      break;
    }
    case OP_RETURN: {
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
    case OP_PRINT: {
      printValue(pop(vm), "", "\n");
      break;
    }
    case OP_POP: {
      pop(vm);
      break;
    }
    case OP_INCREMENT: {
      if (!IS_NUMBER(peek(vm, 0))) {
        runtimeError(vm, "INCREMENT Operation supported only on numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm->stackTop[-1].as.number = vm->stackTop[-1].as.number + 1;
      break;
    }
    case OP_DECREMENT: {
      if (!IS_NUMBER(peek(vm, 0))) {
        runtimeError(vm, "DECREMENT Operation supported only on numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm->stackTop[-1].as.number = vm->stackTop[-1].as.number - 1;
      break;
    }
    case OP_DEFINE_GLOBAL:
    case OP_DEFINE_GLOBAL_LONG: {
      ObjString *name;
      if (instruction == OP_DEFINE_GLOBAL_LONG) {
        name = READ_STRING_LONG();
        MOVE_BYTES(3);
      } else {
        name = READ_STRING();
      }

      // nrk doesn't check for redefinition of global variables, it just
      // overwrites them. This is also useful in repl sessions.
      tableSet(&vm->memoryManager->globals, name, peek(vm, 0));
      pop(vm);
      break;
    }
    case OP_GET_GLOBAL:
    case OP_GET_GLOBAL_LONG: {
      ObjString *name;
      if (instruction == OP_GET_GLOBAL_LONG) {
        name = READ_STRING_LONG();
        MOVE_BYTES(3);
      } else {
        name = READ_STRING();
      }

      Value value;
      if (!tableGet(&vm->memoryManager->globals, name, &value)) {
        runtimeError(vm, "Undefined variable %s", name->str);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(vm, value);
      break;
    }
    case OP_SET_GLOBAL:
    case OP_SET_GLOBAL_LONG: {
      ObjString *name;
      if (instruction == OP_SET_GLOBAL_LONG) {
        name = READ_STRING_LONG();
        MOVE_BYTES(3);
      } else {
        name = READ_STRING();
      }

      // If it's a new entry, it means the variable doesn't exist, clean up and
      // return error.
      if (tableSet(&vm->memoryManager->globals, name, peek(vm, 0))) {
        tableDelete(&vm->memoryManager->globals, name);
        runtimeError(vm, "Undefined variable '%s'.", name->str);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    }
  }

#undef READ_BYTE
#undef MOVE_BYTES
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef READ_STRING
#undef READ_STRING_LONG
#undef BINARY_OP
#undef BINARY_OP_BITWISE
}

InterpretResult interpretChunk(VM *vm, Chunk *chunk) {
  vm->chunk = chunk;
  vm->ip = vm->chunk->code;

  return run(vm);
}

InterpretResult interpret(VM *vm, const char *source) {
  Chunk chunk;
  initChunk(&chunk);
  // TODO: Is this the right place to set the compiler's currentChunk?
  vm->compiler->currentChunk = &chunk;

  if (!compile(vm->compiler, source)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;

  InterpretResult res = run(vm);

  freeChunk(&chunk);
  return res;
}
