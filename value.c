#include <stdio.h>

#include "memory.h"
#include "value.h"

void initValueArray(ValueArray *array) {
  array->cap = 0;
  array->count = 1;
  array->values = NULL;
}

void writeValueArray(ValueArray *array, Value value) {
  if (array->cap < array->count + 1) {
    int oldCap = array->cap;
    array->cap = GROW_CAP(oldCap);
    array->values = GROW_ARR(Value, array->values, oldCap, array->cap);
  }

  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray *array) {
  FREE_ARR(Value, array->values, array->cap);
  initValueArray(array);
}

void printValue(Value value, char *head, char *tail) {
  printf("%s%g%s", head, value, tail);
}
