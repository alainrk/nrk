#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>

// "Constructor" for Object
#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

Obj *allocateObject(size_t size, ObjType type) {
  Obj *obj = (Obj *)reallocate(NULL, 0, size);
  obj->type = type;
  return obj;
}

ObjString *allocateString(char *chars, int length) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->str = chars;
  return string;
}

ObjString *copyString(const char *str, int length) {
  // Allocating and copying over the content, ObjString owns the string on the
  // heap, can free it without consequences and so on.
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, str, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("%s", AS_STRING(value)->str);
    break;
  default:
    printf("Undefined Object Type");
    break;
  }
}

// It needs to be used only when the ownership of the passed in string is
// already under control of who allocates ObjString.
ObjString *takeString(char *str, int len) {
  ObjString *s = allocateString(str, len);
  return s;
}
