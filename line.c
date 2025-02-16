#include <stdio.h>
#include <stdlib.h>

#include "line.h"
#include "memory.h"

void initLineArray(LineArray *array) {
  array->cap = 0;
  array->count = 0;
  array->values = NULL;
}

int emptyLineArray(LineArray *array) { return (array->count == 0); }

// Returns the line given the index of an instruction, -1 otherwise
int getLine(LineArray *array, int idx) {
  // Loop through the linearray and count until the index is reached
  // e.g.
  // [Line 12, Count 3] -> [Line 13, Count 2] -> [Line 15, Count 4]
  // getLine(array, 4) -> Line 13
  int count = 0;

  for (int i = 0; i < array->count; i++) {
    Line curr = array->values[i];

    if (idx >= count && idx < count + curr.count)
      return curr.num;

    count += curr.count;
  }

  return -1;
}

void printLine(LineArray *array) {
  printf("== line ==\n");
  for (int i = 0; i < array->count; i++) {
    printf("(%d): %d\n", array->values[i].num, array->values[i].count);
  }
}

void setLine(LineArray *array, int lineNum) {
  // If there are no lines yet, create the first and init to 1
  if (emptyLineArray(array)) {
    array->cap = GROW_CAP(0);
    array->values = GROW_ARR(Line, array->values, 0, array->cap);
    array->values[0].num = lineNum;
    array->values[0].count = 1;

    array->count = 1;
    return;
  }

  // If the last line inserted is the same of the passed one, just increment it
  if (array->values[array->count - 1].num == lineNum) {
    array->values[array->count - 1].count++;
    return;
  }

  // Otherwise create a new Line
  if (array->cap < array->count + 1) {
    int oldCap = array->cap;
    array->cap = GROW_CAP(oldCap);
    array->values = GROW_ARR(Line, array->values, oldCap, array->cap);
  }

  array->values[array->count].num = lineNum;
  array->values[array->count].count = 1;
  array->count++;
}

void freeLineArray(LineArray *array) {
  FREE_ARR(Line, array->values, array->cap);
  initLineArray(array);
}
