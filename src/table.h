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
  /* Tombstone handling strategy:
   * 1. Tombstones count as "full" when calculating load factor to prevent
   *    potential infinite loops during lookups (we need empty buckets to
   *    terminate searches)
   * 2. We don't decrement count when deleting entries (creating tombstones)
   * 3. We only increment count during insertion when filling a truly empty
   *    bucket (not when replacing existing entries or tombstones)
   *
   * This means 'count' represents (entries + tombstones), not just active
   * entries.
   */
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
ObjString *tableFindString(Table *table, const char *str, int length,
                           uint32_t hash);

#endif
