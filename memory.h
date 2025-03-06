#ifndef nrk_memory_h
#define nrk_memory_h

#include "common.h"

#define GROW_CAP(cap) ((cap) < 8 ? 8 : (cap) * 2)
#define GROW_ARR(type, pointer, oldCount, newCount)                            \
  (type *)reallocate(pointer, sizeof(type) * oldCount, sizeof(type) * newCount)

#define ALLOCATE(type, count)                                                  \
  (type *)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE_ARR(type, pointer, oldCount)                                      \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#endif
