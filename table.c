#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void initTable(Table *table) {
  table->cap = 0;
  table->count = 0;
  table->entries = NULL;
}

void freeTable(Table *table) {
  FREE_ARR(Entry, table->entries, table->cap);
  initTable(table);
}

// Finds the spot in the entries list, with the given key and cap of a table.
// It doesn't take an entire Table struct, so it's possible to search the spot
// in an existing table->entries list, without being contrained to its existing
// capacity.
Entry *findEntry(Entry *entries, int cap, ObjString *key) {
  uint32_t index = key->hash % cap;
  Entry *tombstone = NULL;

  for (;;) {
    Entry *entry = &entries[index];

    // Key found, return the entry.
    if (entry->key == key)
      return entry;

    // Empty entry, go ahead
    if (entry->key == NULL) {
      // If no value set, check first if we have a tombstone saved during the
      // linear probe so far.
      if (IS_NIL(entry->value)) {
        // If we had met a tombstone, return it (e.g. it will be the correct
        // entry where to put the element), otherwise return the current spot as
        // the valid one.
        return tombstone != NULL ? tombstone : entry;
      }

      // Set the tombstone if we haven't already since we are in an empty entry.
      if (tombstone == NULL)
        tombstone = entry;
    }

    // Linear probing, going ahead.
    index = (index + 1) % cap;
  }
}

bool tableGet(Table *table, ObjString *key, Value *value) {
  if (table->count == 0)
    return false;

  Entry *e = findEntry(table->entries, table->cap, key);
  if (e->key == NULL)
    return false;

  *value = e->value;
  return true;
}

void adjustCapacity(Table *table, int cap) {
  Entry *entries = ALLOCATE(Entry, cap);
  if (table->entries == NULL) {
    fprintf(stderr, "Not enough memory to adjust table capacity.\n");
    exit(74);
  }

  // (Re)initialize all the entries
  for (int i = 0; i < cap; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  // If this is an existing table, we need to reposition all the existing
  // entries, as they now would have different position, being the capacity
  // changed.
  //
  // NOTE: Since we don't copy over the tombstones, we recalculate the count.
  table->count = 0;
  for (int i = 0; i < table->cap; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key == NULL)
      continue;

    Entry *dest = findEntry(entries, cap, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARR(Entry, table->entries, table->cap);

  table->entries = entries;
  table->cap = cap;
}

bool tableSet(Table *table, ObjString *key, Value value) {
  if (table->count + 1 > table->cap * TABLE_MAX_LOAD) {
    adjustCapacity(table, GROW_CAP(table->cap));
  }

  Entry *entry = findEntry(table->entries, table->cap, key);

  bool isNew = entry->key == NULL;
  // Tombstone management (see comment in table.h on why they're counted).
  if (isNew && IS_NIL(entry->value))
    table->count++;

  // Upsert info
  entry->key = key;
  entry->value = value;
  return isNew;
}

// Tomblestone method, we leave a signal if deleting an entry, so linear probing
// keeps working.
// Returns false if no element was deleted.
bool tableDelete(Table *table, ObjString *key) {
  Entry *entry = findEntry(table->entries, table->cap, key);
  if (entry == NULL)
    return false;

  entry->key = NULL;
  // Tombstone
  entry->value = BOOL_VAL(true);

  return true;
}

void tableAddAll(Table *from, Table *to) {
  for (int i = 0; i < from->cap; i++) {
    Entry *e = &from->entries[i];
    if (e->key == NULL)
      continue;
    tableSet(to, e->key, e->value);
  }
}
