#ifndef nrk_value_h
#define nrk_value_h

#include "common.h"

// TODO: Only supporting double precision FP numbers for now.
// "Immediate instructions" -> Get saved right after the opcode.
// For variable-sized consts we should save in a separate constant data region
// in the binary exe. The instruction will load the const from the through an
// address/offset pointing to that section. e.g. JVM associate a constant pool
// with each cmd class. We'll have each chunk carrying a list of values
// appearing as literals in the program (specifically all constants will be
// there for simplicity.)
typedef double Value;

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
