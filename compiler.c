#include "compiler.h"
#include "common.h"
#include "scanner.h"
#include "vm.h"
#include <stdio.h>

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

// Returns true is the parser haven't had any error.
bool compile(VM *vm, const char *source, Chunk *chunk) {
  Scanner *scanner = initScanner(source);

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
  // consume(TOKEN_EOF, "Expect end of expression.");

  return !parser.hadError;
}
