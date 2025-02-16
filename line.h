#ifndef nrk_line_h
#define nrk_line_h

#include "common.h"

typedef struct {
  int num;
  int count;
} Line;

typedef struct {
  int cap;
  int count;
  Line *values;
} LineArray;

void initLineArray(LineArray *array);
void freeLineArray(LineArray *array);
void setLine(LineArray *array, int line);
int getLine(LineArray *array, int idx);
void printLine(LineArray *array);

#endif
