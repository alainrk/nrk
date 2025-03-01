#ifndef nrk_value_h
#define nrk_value_h

#include "common.h"

// VM's types, not user's types.
// Types that have the built-in support in the VM.
typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
} ValueType;

// "Immediate instructions" -> Get saved right after the opcode.
//
// For variable-sized consts we should save in a separate constant data region
// in the binary exe. The instruction will load the const from the through an
// address/offset pointing to that section. e.g. JVM associate a constant pool
// with each cmd class.
// We'll have each chunk carrying a list of values
// appearing as literals in the program (specifically all constants will be
// there for simplicity.)
//
// Value struct:
// [..type..|..padding..|.......as.......]
//                       [bool]
//                       [....number....]
typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
  } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

// Conversion from nrk KNOWN values to C values
// It's important to know the type before doing it (see above macros)
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

// Promotion from C values to nrk values.
// Creates a tagged union with value and proper type.
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})

// The constant pool is an array of values.
// The instruction to load a constant looks up the value by index in the array.
typedef struct {
  int cap;
  int count;
  Value *values;
} ValueArray;

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
void printValue(Value value, char *head, char *tail);

#endif
