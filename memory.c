
#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdio.h>
#include <stdlib.h>

MemoryManager *initMemoryManager() {
  MemoryManager *mm = (MemoryManager *)malloc(sizeof(MemoryManager));
  return mm;
}

void freeMemoryManager(MemoryManager *mm) {
  // TODO: Probably there will be other things to cleanup here if it gets more
  // complex.
  free(mm);
}

void freeObject(struct Obj *obj) {
  switch (obj->type) {
  case OBJ_STRING: {
    FREE(Obj, obj);
    // Using Flexible Array Member, we don't need to do this anymore as the
    // free() must be called on the pointer returned on the pointer returned by
    // the single malloc() we do.
    //
    // ObjString *str = (ObjString *)obj;
    // FREE_ARR(char, str->str, str->length + 1);
    // FREE(ObjString, obj);
    break;
  }
  }
}

void freeObjects(MemoryManager *mm) {
  Obj *curr = mm->objects;
  while (curr != NULL) {
    Obj *next = curr->next;
    freeObject(curr);
    curr = next;
  }
}

void *reallocate(void *p, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(p);
    return NULL;
  }

  void *res = realloc(p, newSize);
  if (res == NULL) {
    fprintf(stderr,
            "Error: Failed to reallocate memory - requested from %zu to %zu "
            "realloc bytes\n",
            oldSize, newSize);
    perror("realloc failed");
    exit(1);
  }
  return res;
}
