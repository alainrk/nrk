#ifndef nrk_compiler_h
#define nrk_compiler_h

#include "chunk.h"
#include "memory.h"
#include "scanner.h"

typedef struct {
  bool isLong;
  uint8_t bytes[3]; // For short constant, only bytes[0] is used
                    // For long constant, all three bytes are used
} ConstantIndex;

typedef struct {
  Token curr;
  Token prev;

  // This is useful to avoid cascade of errors. If we have an error we stop
  // right away and inform the user for better debugging.
  bool hadError;

  // Despite we could use setjmp() and longjmp() to have a sort of exceptions it
  // can get very messy. So we just keep track of panic mode through a state
  // flag here.
  bool panicMode;

} Parser;

typedef enum {
  PREC_NONE,       // 0. None
  PREC_ASSIGNMENT, // 1. =
  PREC_OR,         // 2. or
  PREC_AND,        // 3. and
  PREC_EQUALITY,   // 4. == !=
  PREC_COMPARISON, // 5. < > <= >=
  PREC_TERM,       // 6. + -
  PREC_FACTOR,     // 7. * /
  PREC_UNARY,      // 8. ! -
  PREC_CALL,       // 9. . ()
  PREC_PRIMARY     // 10. Primary
} Precedence;

typedef struct {
  MemoryManager *memoryManager;
  Scanner *scanner;
  Parser *parser;
  Chunk *currentChunk;
} Compiler;

typedef void (*ParseFn)(Compiler *compiler, bool canAssign);

// Given an expression starting with a token, knowing its type we need to know:
typedef struct {
  // The func to parse this token when in prefix position (start of expr).
  ParseFn prefix;
  // The func to parse this token when in infix position (between expr).
  ParseFn infix;
  // It's precedence level.
  Precedence precedence;
} ParseRule;

Compiler *initCompiler(MemoryManager *mm);
void freeCompiler(Compiler *compiler);

bool compile(Compiler *compiler, const char *source);
const char *precedenceTypeToString(Precedence type);

#endif
