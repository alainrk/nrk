#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "scanner.h"
#include "vm.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: Probably in this file we're passing around too many parameters. I could
// simply pass around the parser (and maybe something else), and get what I need
// from there.

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

static Chunk *currentChunk(Chunk *chunk) { return chunk; }

static void errorAt(Parser *parser, Token *token, const char *message) {
  // If already panicking, no need to execute further and keep logging easy to
  // read. The flag will be cleared later, as the parser can get lost further
  // until a synchronization point but the user will have all the logs
  // swallowed.
  if (parser->panicMode)
    return;

  parser->panicMode = true;

  fprintf(stderr, "[Line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing
  } else {
    fprintf(stderr, " at '%.*s", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser->hadError = true;
}

static void error(Parser *parser, const char *message) {
  errorAt(parser, &parser->prev, message);
}

static void errorAtCurrent(Parser *parser, const char *message) {
  errorAt(parser, &parser->curr, message);
}

static void advance(Scanner *scanner, Parser *parser) {
  parser->prev = parser->curr;

  for (;;) {
    parser->curr = scanToken(scanner);
    if (parser->curr.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser, parser->curr.start);
  }
}

// Consume just advance but checking that the type is coherent, "throws" an
// error otherwise.
static void consume(Scanner *scanner, Parser *parser, TokenType type,
                    const char *message) {
  if (parser->curr.type == type) {
    advance(scanner, parser);
    return;
  }

  errorAtCurrent(parser, message);
}

static void emitBytes(Parser *parser, Chunk *chunk, int count, ...) {
  va_list args;
  va_start(args, count);

  for (int i = 0; i < count; i++) {
    u_int8_t byte = (u_int8_t)va_arg(args, int);
    writeChunk(currentChunk(chunk), byte, parser->prev.line);
  }

  va_end(args);
}

static void emitReturn(Parser *parser, Chunk *chunk) {
  emitBytes(parser, currentChunk(chunk), 1, OP_RETURN);
}

static void emitConstant(Parser *parser, Chunk *chunk, Value v) {
  int idx = addConstant(currentChunk(chunk), v);

  if (idx <= UINT8_MAX) {
    emitBytes(parser, currentChunk(chunk), 2, OP_CONSTANT, idx);
    return;
  }

  // TODO: Check if correct for 3 bytes at most
  if (idx > 0x00FFFFFE) {
    error(parser, "Too many constants in one chunk.");
    return;
  }

  // Use OP_CONSTANT_LONG and write the 24-bit (3 bytes) applying AND bit by bit
  // for the relevant part and get rid of the rest
  u_int8_t b1 = (idx & 0xff0000) >> 16;
  u_int8_t b2 = (idx & 0x00ff00) >> 8;
  u_int8_t b3 = (idx & 0x0000ff);

  emitBytes(parser, currentChunk(chunk), 4, OP_CONSTANT_LONG, b1, b2, b3);
}

static void endCompiler(Parser *parser, Chunk *chunk) {
  // TODO: Temporarily use OP_RETURN to print values at the end of expressions
  emitReturn(parser, currentChunk(chunk));
}

static void expression() {}

// Prefix expression: We assume "(" has already been consumed.
static void grouping(Scanner *scanner, Parser *parser) {
  expression();
  consume(scanner, parser, TOKEN_RIGHT_PAREN, "Expect ')' after expressions.");
}

// Prefix expression: We assume "(" has already been consumed.
static void unary(Scanner *scanner, Parser *parser, Chunk *chunk) {
  TokenType t = parser->prev.type;

  // Compile the operand
  expression();

  // Emit the operator instruction
  switch (t) {
  case TOKEN_MINUS:
    emitBytes(parser, currentChunk(chunk), 1, OP_NEGATE);
    break;
  default:
    // Unreachable case
    return;
  }
  consume(scanner, parser, TOKEN_RIGHT_PAREN, "Expect ')' after expressions.");
}

static void number(Parser *parser, Chunk *chunk) {
  double v = strtod(parser->prev.start, NULL);
  emitConstant(parser, currentChunk(chunk), v);
}

// Returns true is the parser haven't had any error.
bool compile(VM *vm, const char *source, Chunk *chunk) {
  Scanner *scanner = initScanner(source);
  Chunk *compilingChunk = chunk;

  // TODO: Is it better to init the parser outside and pass it in the compile
  // instead from the vm.c[interpret()]?
  Parser parser;

  parser.hadError = false;
  parser.panicMode = false;

  // int line = -1;
  // for (;;) {
  //   Token token = scanToken(scanner);
  //   if (token.line != line) {
  //     printf("%04d ", token.line);
  //   } else {
  //     printf("    | ");
  //   }
  //
  //   // %.*s allows to print exactly token.length chars as we don't have a \0
  //   // here.
  //   printf("%-25s \"%.*s\"\n", tokenTypeToString(token.type), token.length,
  //          token.start);
  //
  //   if (token.type == TOKEN_ERROR)
  //     break;
  //   if (token.type == TOKEN_EOF)
  //     break;
  // }
  //
  // freeScanner(scanner);
  // return true;

  advance(scanner, &parser);
  // expression();
  consume(scanner, &parser, TOKEN_EOF, "Expect end of expression.");
  endCompiler(&parser, chunk);

  return !parser.hadError;
}
