#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

int debugIndent = 0;

static void grouping(Compiler *compiler);
static void unary(Compiler *compiler);
static void binary(Compiler *compiler);
static void number(Compiler *compiler);
static void literal(Compiler *compiler);
static void string(Compiler *compiler);
static void expression(Compiler *compiler);
static void declaration(Compiler *compiler);
static void statement(Compiler *compiler);
static ParseRule *getRule(TokenType type);

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
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    // TODO:
    // TOKEN_TEMPL_START,        // Opening "`"
    // TOKEN_TEMPL_END,          // Closing "`"
    // TOKEN_TEMPL_INTERP_START, // Opening "${"
    // TOKEN_TEMPL_INTERP_END,   // Closing "}"
    // TOKEN_TEMPL_CONTENT,      // Non-expression content
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
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

Compiler *initCompiler(MemoryManager *mm) {
  // NOTE: Scanner gets initialized in compile() as it take the source code,
  // evaluate if improve it.
  Compiler *compiler = (Compiler *)malloc(sizeof(Compiler));
  compiler->parser = (Parser *)malloc(sizeof(Parser));
  compiler->memoryManager = mm;
  return compiler;
}

void freeCompiler(Compiler *compiler) {
  free(compiler->parser);
  compiler->parser = NULL;

  freeScanner(compiler->scanner);
  compiler->scanner = NULL;

  free(compiler);
}

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
    fprintf(stderr, " at '%.*s'", token->length, token->start);
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

static void advance(Compiler *compiler) {
  compiler->parser->prev = compiler->parser->curr;

  for (;;) {
    compiler->parser->curr = scanToken(compiler->scanner);
    if (compiler->parser->curr.type != TOKEN_ERROR)
      break;

    errorAtCurrent(compiler->parser, compiler->parser->curr.start);
  }
}

// Consume just advance but checking that the type is coherent, "throws" an
// error otherwise.
static void consume(Compiler *compiler, TokenType type, const char *message) {
  if (compiler->parser->curr.type == type) {
    advance(compiler);
    return;
  }

  errorAtCurrent(compiler->parser, message);
}

static bool check(Compiler *compiler, TokenType type) {
  return compiler->parser->curr.type == type;
}

static bool match(Compiler *compiler, TokenType type) {
  if (!check(compiler, type))
    return false;
  advance(compiler);
  return true;
}

static void emitBytes(Compiler *compiler, int count, ...) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%semitBytes(%d) = ",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent), count);
#endif

  va_list args;
  va_start(args, count);

  for (int i = 0; i < count; i++) {
    u_int8_t byte = (u_int8_t)va_arg(args, int);

#ifdef DEBUG_COMPILE_EXECUTION
    printf("%x ", byte);
#endif

    writeChunk(compiler->currentChunk, byte, compiler->parser->prev.line);
  }

  va_end(args);

#ifdef DEBUG_COMPILE_EXECUTION
  printf("\n");
  debugIndent--;
#endif
}

static void emitReturn(Compiler *compiler) {
  emitBytes(compiler, 1, OP_RETURN);
}

static void emitConstant(Compiler *compiler, Value v) {
  int idx = addConstant(compiler->currentChunk, v);

  if (idx <= UINT8_MAX) {
    emitBytes(compiler, 2, OP_CONSTANT, idx);
    return;
  }

  if (idx > 0x00FFFFFE) {
    error(compiler->parser, "Too many constants in one chunk.");
    return;
  }

  // Use OP_CONSTANT_LONG and write the 24-bit (3 bytes) applying AND bit by bit
  // for the relevant part and get rid of the rest
  u_int8_t b1 = (idx & 0xff0000) >> 16;
  u_int8_t b2 = (idx & 0x00ff00) >> 8;
  u_int8_t b3 = (idx & 0x0000ff);

  emitBytes(compiler, 4, OP_CONSTANT_LONG, b1, b2, b3);
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
static void parsePrecedence(Compiler *compiler, Precedence precedence) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sparsePrecedence(%s)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent),
         precedenceTypeToString(precedence));
#endif

  advance(compiler);

  // The first token must always be part of a prefix operation, by definition.
  ParseRule *rule = getRule(compiler->parser->prev.type);
  if (rule->prefix == NULL) {
    error(compiler->parser, "Expect expression");
    return;
  }

#ifdef DEBUG_COMPILE_EXECUTION
  printf("%sprefixRule for %s has precedence = %s\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent),
         tokenTypeToString(compiler->parser->prev.type),
         precedenceTypeToString(rule->precedence));
#endif

  rule->prefix(compiler);

  // If there is some infix rule, the prefix above might an operand of it.
  // Go ahead until, and only if, the precedence allows it.
  while (precedence <= getRule(compiler->parser->curr.type)->precedence) {
    advance(compiler);
    ParseRule *r = getRule(compiler->parser->prev.type);

#ifdef DEBUG_COMPILE_EXECUTION
    printf("%sinfixRule for %s has precedence = %s\n",
           strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent),
           tokenTypeToString(compiler->parser->prev.type),
           precedenceTypeToString(r->precedence));
#endif

    r->infix(compiler);
  }

#ifdef DEBUG_COMPILE_EXECUTION
  printf("%s end parsePrecedence()\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent));
  debugIndent--;
#endif
}

static void endCompiler(Compiler *compiler) {
  emitReturn(compiler);

#ifdef DEBUG_PRINT_CODE
  if (!compiler->parser->hadError) {
    disassembleChunk(compiler->currentChunk, "code");
  }
#endif
}

ParseRule *getRule(TokenType t) { return &rules[t]; }

static void binary(Compiler *compiler) {
  TokenType t = compiler->parser->prev.type;

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
  parsePrecedence(compiler, (Precedence)(rule->precedence + 1));

  switch (t) {
  case TOKEN_PLUS:
    emitBytes(compiler, 1, OP_ADD);
    break;
  case TOKEN_MINUS:
    emitBytes(compiler, 1, OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitBytes(compiler, 1, OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitBytes(compiler, 1, OP_DIVIDE);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitBytes(compiler, 1, OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitBytes(compiler, 1, OP_GREATER);
    break;
  case TOKEN_LESS:
    emitBytes(compiler, 1, OP_LESS);
    break;
  case TOKEN_BANG_EQUAL:
    // This could be done  by pushing EQUAL, NOT to reduce the amount of ops,
    // but this way is slightly faster
    emitBytes(compiler, 1, OP_NOT_EQUAL);
    break;
  case TOKEN_GREATER_EQUAL:
    // This could be done by pushing LESS, NOT to reduce the amount of ops, but
    // this way is slightly faster
    emitBytes(compiler, 1, OP_GREATER_EQUAL);
    break;
  case TOKEN_LESS_EQUAL:
    // This could be done by pushing GREATER, NOT to reduce the amount of ops,
    // but this way is slightly faster
    emitBytes(compiler, 1, OP_LESS_EQUAL);
    break;

  // Unreachable case
  default:
    return;
  }
}

static void expression(Compiler *compiler) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sexpression()\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent));
#endif

  // This way we parse all the possible expression, being ASSIGNMENT the lowest.
  parsePrecedence(compiler, PREC_ASSIGNMENT);

#ifdef DEBUG_COMPILE_EXECUTION
  printf("%send expression()\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent));
  debugIndent--;
#endif
}

static void printStatement(Compiler *compiler) {
  expression(compiler);
  consume(compiler, TOKEN_SEMICOLON, "Expect ';' after value.");
  emitBytes(compiler, 1, OP_PRINT);
}

static void declaration(Compiler *compiler) { statement(compiler); }

static void statement(Compiler *compiler) {
  if (match(compiler, TOKEN_PRINT)) {
    printStatement(compiler);
  }
}

// Prefix expression: We assume "(" has already been consumed.
static void grouping(Compiler *compiler) {

#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sgrouping(%s)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent),
         tokenTypeToString(TOKEN_RIGHT_PAREN));
  debugIndent--;
#endif

  // This will generate all the necessary bytecode needed to evaluate the
  // expression.
  expression(compiler);
  consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after expressions.");
}

// Prefix expression: We assume "(" has already been consumed.
static void unary(Compiler *compiler) {
  TokenType t = compiler->parser->prev.type;

#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sunary(%s)\n", strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent),
         tokenTypeToString(t));
  debugIndent--;
#endif

  // Compile the operand.
  parsePrecedence(compiler, PREC_UNARY);

  // Emit the operator instruction, AFTER the expression, so it gets then popped
  // and the operator applied.
  switch (t) {
    // When parsing the operand to unary -, we need to compile only expressions
    // at a certain precedence level or higher.
  case TOKEN_MINUS:
    emitBytes(compiler, 1, OP_NEGATE);
    break;
  case TOKEN_BANG:
    emitBytes(compiler, 1, OP_NOT);
    break;
  // Unreachable case
  default:
    error(compiler->parser, "Unexpected unary");
    return;
  }
}

static void number(Compiler *compiler) {
  double v = strtod(compiler->parser->prev.start, NULL);

#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%snumber(%.2f)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent), v);
  debugIndent--;
#endif

  emitConstant(compiler, NUMBER_VAL(v));
}

static void string(Compiler *compiler) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sstring(%s)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent),
         tokenTypeToString(compiler->parser->prev.type));
  debugIndent--;
#endif

  // We need to copy the string from the source code into our heap, starting
  // right after the `"` and before the ending `"`, and including space for \0
  // that's not in the source code.
  emitConstant(compiler,
               OBJ_VAL(copyString(compiler->memoryManager,
                                  compiler->parser->prev.start + 1,
                                  compiler->parser->prev.length - 2)));
}

static void literal(Compiler *compiler) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sliteral(%s)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent),
         tokenTypeToString(compiler->parser->prev.type));
  debugIndent--;
#endif

  switch (compiler->parser->prev.type) {
  case TOKEN_NIL:
    emitBytes(compiler, 1, OP_NIL);
    break;
  case TOKEN_TRUE:
    emitBytes(compiler, 1, OP_TRUE);
    break;
  case TOKEN_FALSE:
    emitBytes(compiler, 1, OP_FALSE);
    break;
  default:
    error(compiler->parser, "Unexpected literal");
    return;
  }
}

// Returns true is the parser haven't had any error.
bool compile(Compiler *compiler, const char *source) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent = 0;
  printf("======= compile start() =======\n\n");
#endif

  // TODO: Move this in an upper level initialization and remember to call
  // freeXXX functions of them too
  compiler->scanner = initScanner(source);

  compiler->parser->hadError = false;
  compiler->parser->panicMode = false;

  advance(compiler);

  while (!match(compiler, TOKEN_EOF)) {
    declaration(compiler);
  }

  // expression(compiler, chunk);
  // consume(compiler, TOKEN_EOF, "Expect end of expression.");

  endCompiler(compiler);

#ifdef DEBUG_COMPILE_EXECUTION
  printf("\n======== compile end() ========\n\n");
#endif

  return !compiler->parser->hadError;
}
