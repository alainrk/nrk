#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

int debugIndent = 0;

static void grouping(Compiler *compiler, bool canAssign);
static void unary(Compiler *compiler, bool canAssign);
static void binary(Compiler *compiler, bool canAssign);
static void number(Compiler *compiler, bool canAssign);
static void literal(Compiler *compiler, bool canAssign);
static void string(Compiler *compiler, bool canAssign);
static void variable(Compiler *compiler, bool canAssign);
static void postfix(Compiler *compiler, bool canAssign);

static void expression(Compiler *compiler);
static void declaration(Compiler *compiler);
static void statement(Compiler *compiler);

static ParseRule *getRule(TokenType type);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, NULL, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, NULL, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, NULL, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, NULL, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, NULL, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, NULL, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, NULL, PREC_NONE},
    // TODO: Set functions and precedence to these six:
    [TOKEN_PLUS_PLUS] = {NULL, NULL, postfix, PREC_UNARY},
    [TOKEN_MINUS_MINUS] = {NULL, NULL, postfix, PREC_UNARY},
    [TOKEN_PLUS_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_MINUS_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_STAR_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_SLASH_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    // TODO:
    // TOKEN_TEMPL_START,        // Opening "`"
    // TOKEN_TEMPL_END,          // Closing "`"
    // TOKEN_TEMPL_INTERP_START, // Opening "${"
    // TOKEN_TEMPL_INTERP_END,   // Closing "}"
    // TOKEN_TEMPL_CONTENT,      // Non-expression content
    [TOKEN_NUMBER] = {number, NULL, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, NULL, PREC_NONE},
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

// Emit the correct number of bytes depending on how many there are for the
// given constant index.
static void emitConstantIndex(Compiler *compiler, ConstantIndex index,
                              OpCode codeShort, OpCode codeLong) {
  if (index.isLong) {
    emitBytes(compiler, 4, codeLong, index.bytes[0], index.bytes[1],
              index.bytes[2]);
    return;
  }

  emitBytes(compiler, 2, codeShort, index.bytes[0]);
}

static void emitReturn(Compiler *compiler) {
  emitBytes(compiler, 1, OP_RETURN);
}

ConstantIndex makeConstant(Compiler *compiler, Value v) {
  ConstantIndex cidx;
  // Avoid compiler complaining.
  cidx.isLong = false;

  int idx = addConstant(compiler->currentChunk, v);

  if (idx <= UINT8_MAX) {
    cidx.bytes[0] = idx;
    cidx.isLong = false;
    return cidx;
  }

  if (idx > 0x00FFFFFE) {
    error(compiler->parser, "Too many constants in one chunk.");
    return cidx;
  }

  // Use OP_CONSTANT_LONG and write the 24-bit (3 bytes) applying AND bit by bit
  // for the relevant part and get rid of the rest
  cidx.bytes[0] = (idx & 0xff0000) >> 16;
  cidx.bytes[1] = (idx & 0x00ff00) >> 8;
  cidx.bytes[2] = (idx & 0x0000ff);
  cidx.isLong = true;

  return cidx;
}

static void emitConstant(Compiler *compiler, Value v) {
  ConstantIndex cidx = makeConstant(compiler, v);
  emitConstantIndex(compiler, cidx, OP_CONSTANT, OP_CONSTANT_LONG);
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

  // variable() should look for and consume the '=' only if it’s in the context
  // of a low-precedence expression.
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  rule->prefix(compiler, canAssign);

  // Process any postfix operations immediately
  while (true) {
    rule = getRule(compiler->parser->curr.type);
    if (rule->postfix == NULL || precedence > rule->precedence)
      break;
    advance(compiler);
    rule->postfix(compiler, canAssign);
  }

  // If there is some infix rule, the prefix above might be an operand of it.
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

    r->infix(compiler, canAssign);
  }

  if (canAssign && match(compiler, TOKEN_EQUAL)) {
    error(compiler->parser, "Invalid assignment target.");
  }

#ifdef DEBUG_COMPILE_EXECUTION
  printf("%s end parsePrecedence()\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent));
  debugIndent--;
#endif
}

// Adds the variable (identifier) to the chunk's constants table as a string,
// returning its index (normal or long).
ConstantIndex identifierConstant(Compiler *compiler, Token *name) {
  return makeConstant(compiler, OBJ_VAL(copyString(compiler->memoryManager,
                                                   name->start, name->length)));
}

ConstantIndex parseVariable(Compiler *compiler, const char *message) {
  consume(compiler, TOKEN_IDENTIFIER, message);
  return identifierConstant(compiler, &compiler->parser->prev);
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

static void binary(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);

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

// Defining a variable, means putting its name into a ObjString, but storing the
// index in the VM operation with its index in the Chunk's constants table, as
// otherwise would be too expensive.
static void defineVariable(Compiler *compiler, ConstantIndex variable) {
  emitConstantIndex(compiler, variable, OP_DEFINE_GLOBAL,
                    OP_DEFINE_GLOBAL_LONG);
}

// Variable declaration, parsing it and storing in constant's table (see
// defineVariable() for the details).
static void varDeclaration(Compiler *compiler) {
  ConstantIndex global = parseVariable(compiler, "Expect variable name.");

  if (match(compiler, TOKEN_EQUAL)) {
    // Get the variable value.
    expression(compiler);
  } else {
    // Otherwise empty initialization, set nil.
    // Syntactic sugar for `var a = nil`;
    emitBytes(compiler, 1, OP_NIL);
  }

  consume(compiler, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(compiler, global);
}

static void expressionStatement(Compiler *compiler) {
  expression(compiler);
  consume(compiler, TOKEN_SEMICOLON, "Expect ';' after value.");
  emitBytes(compiler, 1, OP_POP);
}

static void printStatement(Compiler *compiler) {
  expression(compiler);
  consume(compiler, TOKEN_SEMICOLON, "Expect ';' after value.");
  emitBytes(compiler, 1, OP_PRINT);
}

// Synchronization phase, avoid in case of error of propagating.
// Skip every token until we come to a possible end of statement token.
static void synchronize(Compiler *compiler) {
  compiler->parser->panicMode = false;

  while (compiler->parser->curr.type != TOKEN_EOF) {
    if (compiler->parser->prev.type == TOKEN_SEMICOLON)
      break;
    switch (compiler->parser->curr.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      // End of statement, return.
      return;
      // Do nothing here.
    default:;
    }

    advance(compiler);
  }
}

static void declaration(Compiler *compiler) {
  if (match(compiler, TOKEN_VAR)) {
    varDeclaration(compiler);
  } else {
    statement(compiler);
  }

  if (compiler->parser->panicMode) {
    synchronize(compiler);
  }
}

static void statement(Compiler *compiler) {
  if (match(compiler, TOKEN_PRINT)) {
    printStatement(compiler);
  } else {
    expressionStatement(compiler);
  }
}

// Prefix expression: We assume "(" has already been consumed.
static void grouping(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);

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
static void unary(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);

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

static void number(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);

  double v = strtod(compiler->parser->prev.start, NULL);

#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%snumber(%.2f)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent), v);
  debugIndent--;
#endif

  emitConstant(compiler, NUMBER_VAL(v));
}

static void string(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);

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

static void namedVariable(Compiler *compiler, Token *name, bool canAssign) {
  ConstantIndex cidx = identifierConstant(compiler, name);

  // If we are on a "=", this is a setter, so we consume the expression, if
  // possible.
  if (canAssign && match(compiler, TOKEN_EQUAL)) {
    expression(compiler);
    emitConstantIndex(compiler, cidx, OP_SET_GLOBAL, OP_SET_GLOBAL_LONG);
  } else {
    emitConstantIndex(compiler, cidx, OP_GET_GLOBAL, OP_GET_GLOBAL_LONG);
  }
}

// Variable read access (e.g. to read its value in an expression).
static void variable(Compiler *compiler, bool canAssign) {
  namedVariable(compiler, &compiler->parser->prev, canAssign);
}

static void postfix(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);

  Chunk *currChunk = compiler->currentChunk;

  // At this point, the variable value is already on the stack
  // Due to the GET_GLOBAL(_LONG) emitted by the variable prefix function

  // Check that we have at least compiled the variable operator.
  if (currChunk->count < 2) {
    error(compiler->parser, "Invalid target for postfix operator");
    return;
  }

  // Check if the previous op was a variable load
  uint8_t lastOp = currChunk->code[currChunk->count - 2];

  if (lastOp != OP_GET_GLOBAL && lastOp != OP_GET_GLOBAL_LONG) {
    error(compiler->parser, "Can only apply postfix operators to a variable");
    return;
  }

  ConstantIndex varIndex;

  if (lastOp == OP_GET_GLOBAL) {
    // Short constant index case
    varIndex.bytes[0] = currChunk->code[currChunk->count - 1];
    varIndex.isLong = false;
  } else {
    // Long constant index case
    varIndex.bytes[0] = currChunk->code[currChunk->count - 3];
    varIndex.bytes[1] = currChunk->code[currChunk->count - 2];
    varIndex.bytes[2] = currChunk->code[currChunk->count - 1];
    varIndex.isLong = true;
  }

  // Duplicate the value on the stack
  emitBytes(compiler, 1, __OP_DUP);

  // Determine the operation based on the token type
  switch (compiler->parser->prev.type) {
  case TOKEN_PLUS_PLUS:
    // Add 1 to the duplicate
    emitConstant(compiler, NUMBER_VAL(1));
    emitBytes(compiler, 1, OP_ADD);
    break;

  case TOKEN_MINUS_MINUS:
    // Subtract 1 from the duplicate
    emitConstant(compiler, NUMBER_VAL(1));
    emitBytes(compiler, 1, OP_SUBTRACT);
    break;

  default:
    error(compiler->parser, "Unknown postfix operator");
    return;
  }

  // Store back to the variable
  emitConstantIndex(compiler, varIndex, OP_SET_GLOBAL, OP_SET_GLOBAL_LONG);

  // Pop the stored value, leaving the original
  emitBytes(compiler, 1, OP_POP);
}

static void literal(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);

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
