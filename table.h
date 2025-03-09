#ifndef nrk_table_h
#define nrk_table_h

#include "common.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

typedef struct {
  ObjString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int cap;
  Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);
bool tableSet(Table *table, ObjString *key, Value value);
bool tableGet(Table *table, ObjString *key, Value *value);
bool tableDelete(Table *table, ObjString *key);
void tableAddAll(Table *from, Table *to);

#endif
