#ifndef nrk_object_h
#define nrk_object_h

#include "common.h"
#include "memory.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

// A function is needed as we need to use "value" twice, meaning it can
// duplicate side-effects.
#define IS_STRING(value) isObjType(value, OBJ_STRING)

// Returns the ObjString*
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
// Returns the underlying chars array in ObjString*
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->str)

typedef enum {
  OBJ_STRING,
} ObjType;

// We use a kind "Type Punning", (in this case Nystrom calls it Struct
// Injeritance). Using the mandatory C alignment of the first element in struct,
// we are sure that Obj will be at the beginning, this allows some kind of
// "inheritance". So we can take a struct pointer and convert it to its first
// element's pointer (and viceversa) => i.e. upcasting/downcasting it's possible
// this way.
// Obj will be always the first element in every struct ObjXXX.
struct Obj {
  ObjType type;
  // For now we use intrusive list for GC
  Obj *next;
};

// ObjString uses Flexible Array Member pattern to be more performant, allowing
// not to have two separate memory allocation (obj and str).
struct ObjString {
  Obj obj;
  int length;
  uint32_t hash;
  // Flexible array member: must be at the end.
  char str[];
};

ObjString *copyString(MemoryManager *mm, const char *str, int length);
void printObject(Value value);
ObjString *takeString(MemoryManager *mm, char *str, int len);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
