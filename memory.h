#ifndef nrk_memory_h
#define nrk_memory_h

#include "common.h"
#include "table.h"
#include "value.h"

#define GROW_CAP(cap) ((cap) < 8 ? 8 : (cap) * 2)
#define GROW_ARR(type, pointer, oldCount, newCount)                            \
  (type *)reallocate(pointer, sizeof(type) * oldCount, sizeof(type) * newCount)

#define ALLOCATE(type, count)                                                  \
  (type *)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE_ARR(type, pointer, oldCount)                                      \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

typedef struct {
  // Garbage collector management on created objects
  Obj *objects;

  // Interned strings
  Table strings;
} MemoryManager;

MemoryManager *initMemoryManager();
void freeMemoryManager(MemoryManager *mm);
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#endif
