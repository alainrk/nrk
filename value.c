#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
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
  printf("%s", head);
  switch (value.type) {
  case VAL_NIL:
    printf("nil");
    break;
  case VAL_BOOL:
    printf("%s", AS_BOOL(value) ? "true" : "false");
    break;
  case VAL_NUMBER:
    printf("%g", AS_NUMBER(value));
    break;
  case VAL_OBJ:
    printObject(value);
    break;
  default:
    printf("printValue() not defined on given type");
  }
  printf("%s", tail);
}

bool valuesEqual(Value a, Value b) {
  if (a.type != b.type)
    return false;
  switch (a.type) {
  case VAL_NUMBER:
    return AS_NUMBER(a) == AS_NUMBER(b);
    break;
  case VAL_BOOL:
    return AS_BOOL(a) == AS_BOOL(b);
    break;
  case VAL_NIL:
    return true;
  case VAL_OBJ: {
    ObjString *as = AS_STRING(a);
    ObjString *bs = AS_STRING(b);
    return as->length == bs->length && memcmp(as, bs, as->length) == 0;
  }
  default:
    return false;
  }
}
