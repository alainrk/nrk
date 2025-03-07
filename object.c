#include "object.h"
#include "memory.h"
#include "value.h"
#include <stdio.h>
#include <string.h>

// "Constructor" for Object
#define ALLOCATE_OBJ(mm, type, objectType)                                     \
  (type *)allocateObject(mm, sizeof(type), objectType)

Obj *allocateObject(MemoryManager *mm, size_t size, ObjType type) {
  Obj *obj = (Obj *)reallocate(NULL, 0, size);
  obj->type = type;

  // Adding to the head of the LinkedList for GC
  obj->next = mm->objects;
  // Update the head
  mm->objects = obj;

  return obj;
}

ObjString *allocateString(MemoryManager *mm, char *chars, int length) {
  ObjString *string = ALLOCATE_OBJ(mm, ObjString, OBJ_STRING);
  string->length = length;
  string->str = chars;
  return string;
}

ObjString *copyString(MemoryManager *mm, const char *str, int length) {
  // Allocating and copying over the content, ObjString owns the string on the
  // heap, can free it without consequences and so on.
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, str, length);
  heapChars[length] = '\0';
  return allocateString(mm, heapChars, length);
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
ObjString *takeString(MemoryManager *mm, char *str, int len) {
  ObjString *s = allocateString(mm, str, len);
  return s;
}
