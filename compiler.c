#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "scanner.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER] = {NULL, NULL, PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS] = {NULL, NULL, PREC_NONE},
    [TOKEN_LESS_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {NULL, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

const char *precedenceTypeToString(Precedence type) {
  switch (type) {
  case PREC_NONE:
    return "PREC_NONE";
    break;
  case PREC_ASSIGNMENT:
    return "PREC_ASSIGNMENT";
    break;
  case PREC_OR:
    return "PREC_OR";
    break;
  case PREC_AND:
    return "PREC_AND";
    break;
  case PREC_EQUALITY:
    return "PREC_EQUALITY";
    break;
  case PREC_COMPARISON:
    return "PREC_COMPARISON";
    break;
  case PREC_TERM:
    return "PREC_TERM";
    break;
  case PREC_FACTOR:
    return "PREC_FACTOR";
    break;
  case PREC_UNARY:
    return "PREC_UNARY";
    break;
  case PREC_CALL:
    return "PREC_CALL";
    break;
  case PREC_PRIMARY:
    return "PREC_PRIMARY";
    break;
  default: {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "UNKNOWN_PRECEDENCE(%d)", type);
    return buffer;
  }
  }
}

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

// Parses the expression with given precedence or higher.
// e.g. `-a.b + c`
// If we call parsePrecedence(PREC_ASSIGNMENT), then it will parse the entire
// expression because + has higher precedence than assignment. If instead we
// call parsePrecedence(PREC_UNARY), it will compile the -a.b and stop there. It
// doesn’t keep going through the + because the addition has lower precedence
// than unary operators.
//
// Flow:
//
// 1. expression() -> parsePrecedence()
//  2. parsePrecedence() -> getRule()
//   3. getRule() -> ParserTable
//    4. ParserTable -> binary() / unary() / grouping() / number()
//     - unary() -> parsePrecedence()
//     - binary() -> parsePrecedence()
//                -> getRule()
//     - grouping() -> expression()
//     - number() -> Consume byte.
//
static void parsePrecedence(Scanner *scanner, Parser *parser,
                            Precedence precedence, Chunk *chunk) {
#ifdef DEBUG_COMPILE_EXECUTION
  printf("===== parsePrecedence(%s) =====\n",
         precedenceTypeToString(precedence));
#endif

  advance(scanner, parser);

  // The first token must always be part of a prefix operation, by definition.
  ParseFn prefixRule = getRule(parser->prev.type)->prefix;
  if (prefixRule == NULL) {
    error(parser, "Expect expression");
    return;
  }

  prefixRule(scanner, parser, chunk);

  // If there is some infix rule, the prefix above might an operand of it.
  // Go ahead until, and only if, the precedence allows it.
  while (precedence <= getRule(parser->curr.type)->precedence) {
    advance(scanner, parser);
    ParseFn infixRule = getRule(parser->prev.type)->infix;
    infixRule(scanner, parser, chunk);
  }

#ifdef DEBUG_COMPILE_EXECUTION
  printf("=== end parsePrecedence() ===\n");
#endif
}

static void endCompiler(Parser *parser, Chunk *chunk) {
  emitReturn(parser, currentChunk(chunk));

#ifdef DEBUG_PRINT_CODE
  if (!parser->hadError) {
    disassembleChunk(chunk, "code");
  }
#endif
}

ParseRule *getRule(TokenType t) { return &rules[t]; }

static void binary(Scanner *scanner, Parser *parser, Chunk *chunk) {
  TokenType t = parser->prev.type;

  // Example: 2 * 3 + 4
  //
  // When we parse the right operand of the * expression, we need to just
  // capture 3, and not 3 + 4, because + is lower precedence than *.
  //
  // Instead of defining a separate function for each binary operator, that
  // would call parsePrecedence() and pass in the correct precedence level, we
  // can consider that each binary operator’s right-hand operand precedence is
  // one level higher than its own.
  //
  // So we can look that up dynamically with getRule(), calling then
  // parsePrecedence() with one level higher than this operator’s level.
  //
  // ---
  //
  // NOTE We use one higher level of precedence for the right operand because
  // the binary operators are left-associative.
  //
  // Given a series of the same operator, like: 1 + 2 + 3 + 4.
  // We want to parse it like: ((1 + 2) + 3) + 4.
  //
  // Thus, when parsing the right-hand operand to the first +, we want to
  // consume the 2, but not the rest, so we use one level above +’s precedence.
  //
  // But if our operator was right-associative, this would be wrong.
  //
  // Given: a = b = c = d.
  // We need to parse it like: a = (b = (c = d)).
  //
  // To do that we'd call parsePrecedence with the same precedence instead.

  ParseRule *rule = getRule(t);
  parsePrecedence(scanner, parser, (Precedence)(rule->precedence + 1), chunk);

  switch (t) {
  case TOKEN_PLUS:
    emitBytes(parser, currentChunk(chunk), 1, OP_ADD);
    break;
  case TOKEN_MINUS:
    emitBytes(parser, currentChunk(chunk), 1, OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitBytes(parser, currentChunk(chunk), 1, OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitBytes(parser, currentChunk(chunk), 1, OP_DIVIDE);
    break;
  // Unreachable case
  default:
    return;
  }
}

static void expression(Scanner *scanner, Parser *parser, Chunk *chunk) {
#ifdef DEBUG_COMPILE_EXECUTION
  printf("===== expression() =====\n");
#endif

  // This way we parse all the possible expression, being ASSIGNMENT the lowest.
  parsePrecedence(scanner, parser, PREC_ASSIGNMENT, chunk);

#ifdef DEBUG_COMPILE_EXECUTION
  printf("=== end expression() ===\n");
#endif
}

// Prefix expression: We assume "(" has already been consumed.
static void grouping(Scanner *scanner, Parser *parser, Chunk *chunk) {

#ifdef DEBUG_COMPILE_EXECUTION
  printf("== grouping() ==\n%s\n================\n",
         tokenTypeToString(TOKEN_RIGHT_PAREN));
#endif

  // This will generate all the necessary bytecode needed to evaluate the
  // expression.
  expression(scanner, parser, chunk);
  consume(scanner, parser, TOKEN_RIGHT_PAREN, "Expect ')' after expressions.");
}

// Prefix expression: We assume "(" has already been consumed.
static void unary(Scanner *scanner, Parser *parser, Chunk *chunk) {
  TokenType t = parser->prev.type;

#ifdef DEBUG_COMPILE_EXECUTION
  printf("== unary() ==\n%s\n=============\n", tokenTypeToString(t));
#endif

  // Compile the operand.
  parsePrecedence(scanner, parser, PREC_UNARY, chunk);

  // Emit the operator instruction, AFTER the expression, so it gets then popped
  // and the operator applied.
  switch (t) {
    // When parsing the operand to unary -, we need to compile only expressions
    // at a certain precedence level or higher.
  case TOKEN_MINUS:
    emitBytes(parser, currentChunk(chunk), 1, OP_NEGATE);
    break;
  // Unreachable case
  default:
    return;
  }
}

static void number(Scanner *scanner, Parser *parser, Chunk *chunk) {
  double v = strtod(parser->prev.start, NULL);

#ifdef DEBUG_COMPILE_EXECUTION
  printf("== number() ==\n%.2f\n==============\n", v);
#endif

  emitConstant(parser, currentChunk(chunk), v);
}

// Returns true is the parser haven't had any error.
bool compile(const char *source, Chunk *chunk) {
#ifdef DEBUG_COMPILE_EXECUTION
  printf("======= compile start() =======\n");
#endif

  Scanner *scanner = initScanner(source);
  Parser parser;

  parser.hadError = false;
  parser.panicMode = false;

  advance(scanner, &parser);
  expression(scanner, &parser, chunk);
  consume(scanner, &parser, TOKEN_EOF, "Expect end of expression.");
  endCompiler(&parser, chunk);

#ifdef DEBUG_COMPILE_EXECUTION
  printf("======== compile end() ========\n");
#endif

  return !parser.hadError;
}
