#ifndef nrk_object_h
#define nrk_object_h

#include "common.h"
#include "value.h"

typedef enum {
  OBJ_STRING,
} ObjType;

// We use a kind "Type Punning", (in this case Nystrom calls it Struct
// Injeritance). Using the mandatory C alignment of the first element in struct,
// we are sure that Obj will be at the beginning, this allows some kind of
// "inheritance". So we can take a struct pointer and convert it to it's first
// element's pointer (and viceversa) => i.e. upcasting/downcasting it's possible
// this way.
// Obj will be always the first element in every struct ObjXXX.
struct Obj {
  ObjType type;
};

struct ObjString {
  Obj obj;
  int len;
  char *str;
};

#endif
