
#include "memory.h"
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
