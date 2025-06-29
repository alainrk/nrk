#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"
#include "table.h"
#include "value.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    [TOKEN_PLUS_PLUS] = {NULL, NULL, postfix, PREC_UNARY},
    [TOKEN_MINUS_MINUS] = {NULL, NULL, postfix, PREC_UNARY},
    [TOKEN_PLUS_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_MINUS_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_STAR_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    [TOKEN_SLASH_EQUAL] = {NULL, NULL, NULL, PREC_NONE},
    // TODO: Set functions and precedence to these:
    [TOKEN_GREATER_GREATER] = {NULL, binary, NULL, PREC_TERM},
    [TOKEN_LESS_LESS] = {NULL, NULL, binary, PREC_TERM},
    [TOKEN_AMPERSEND] = {NULL, binary, NULL, PREC_TERM},
    [TOKEN_CARET] = {NULL, binary, NULL, PREC_TERM},
    [TOKEN_PIPE] = {NULL, binary, NULL, PREC_TERM},
    [TOKEN_TILDE] = {unary, NULL, NULL, PREC_UNARY},
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
    [TOKEN_CONST] = {NULL, NULL, NULL, PREC_NONE},
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
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
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

static int emitJump(Compiler *compiler, u_int8_t instruction) {
  emitBytes(compiler, 1, instruction);
  emitBytes(compiler, 2, 0xff, 0xff);
  return compiler->currentChunk->count - 2;
}

static void patchJump(Compiler *compiler, int offset) {
  // -2 to adjust for the bytecode of the jump offset.
  int jump = compiler->currentChunk->count - offset - 2;
  if (jump > UINT16_MAX) {
    error(compiler->parser, "Too much code to jump over.");
  }
  compiler->currentChunk->code[offset] = (jump >> 8) & 0xff;
  compiler->currentChunk->code[offset + 1] = jump & 0xff;
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

bool identifiersEqual(Token *a, Token *b) {
  return ((a->length == b->length) &&
          (memcmp(a->start, b->start, a->length) == 0));
}

static int resolveLocal(Compiler *compiler, Token *name) {
  // Loop backward passing all the scope bottom up to resolve the first matching
  // one.
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      // When we resolve to a local variable we check the scope depth to see if
      // it's fully defined (usage in `var a = a+3;`).
      if (local->depth == -1) {
        error(compiler->parser, "Can't read variable in it's own initializer.");
      }
      return i;
    }
  }

  return -1;
}

// Records the existence of temporary local variable in the compiler.
static void addLocal(Compiler *compiler, Token name, bool isConstant) {
  if (compiler->localCount >= UINT8_COUNT) {
    error(compiler->parser, "Too many local variables in function.");
    return;
  }

  Local *local = &compiler->locals[compiler->localCount++];
  local->name = name;
  // We are initializing the variable, and we need to prevent its usage in the
  // expression, see defineVariable() comment in the if statement for details.
  local->depth = -1;
  local->isConst = isConstant;
}

// Declare: when a variable is added to the scope (define is when it's ready to
// use).
static void declareVariable(Compiler *compiler, bool isConstant) {
  // Global variable, just return as it's late bound and present in global
  // table.
  if (compiler->scopeDepth == 0)
    return;

  // Local variable.
  Token *name = &compiler->parser->prev;

  // Not allowing redeclaring the same variable name in the same scope.
  // e.g.
  // {
  //  var a = 1;
  //  var a = 2;
  // }
  //
  // We start from the end as in the compiler the last scope is on top, so we
  // can iterate backward and stop when reached the end on our scope.
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (local->depth != -1 && local->depth < compiler->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error(compiler->parser,
            "Already a variable with this name in this scope.");
    }
  }

  addLocal(compiler, *name, isConstant);
}

ConstantIndex parseVariable(Compiler *compiler, const char *message,
                            bool isConstant) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sparseVariable\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent));
  debugIndent--;
#endif

  consume(compiler, TOKEN_IDENTIFIER, message);
  declareVariable(compiler, isConstant);

  // If it's a local variable we don't really care as it will remain on the
  // stack, so the index won't be used to lookup in global table.
  if (compiler->scopeDepth > 0) {
    ConstantIndex res = {.isLong = false, {0, 0, 0}};
    return res;
  }

  return identifierConstant(compiler, &compiler->parser->prev);
}

// See description comment in defineVariable() on why we need this.
static void markInitialized(Compiler *compiler) {
  compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

static void endCompiler(Compiler *compiler) {
  emitReturn(compiler);

#ifdef DEBUG_PRINT_CODE
  if (!compiler->parser->hadError) {
    disassembleChunk(compiler->currentChunk, "code");
  }
#endif
}

// Increment the current scope depth of the compiler.
static void beginScope(Compiler *compiler) { compiler->scopeDepth++; }

// Decrement the current scope depth of the compiler and empty the stack from
// the remaining local variables.
static void endScope(Compiler *compiler) {
  compiler->scopeDepth--;

  // TODO: Implement a OP_POP_N operator that pops N elements for performance.
  while (compiler->localCount > 0 &&
         compiler->locals[compiler->localCount - 1].depth >
             compiler->scopeDepth) {
    emitBytes(compiler, 1, OP_POP);
    compiler->localCount--;
  }
}

ParseRule *getRule(TokenType t) { return &rules[t]; }

static void binary(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);

#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sbinary(%d)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent), canAssign);
  debugIndent--;
#endif

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
    emitBytes(compiler, 1, OP_NOT_EQUAL);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(compiler, 1, OP_GREATER_EQUAL);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(compiler, 1, OP_LESS_EQUAL);
    break;
  case TOKEN_GREATER_GREATER:
    emitBytes(compiler, 1, OP_BITWISE_SHIFT_RIGHT);
    break;
  case TOKEN_LESS_LESS:
    emitBytes(compiler, 1, OP_BITWISE_SHIFT_LEFT);
    break;
  case TOKEN_AMPERSEND:
    emitBytes(compiler, 1, OP_BITWISE_AND);
    break;
  case TOKEN_PIPE:
    emitBytes(compiler, 1, OP_BITWISE_OR);
    break;
  case TOKEN_CARET:
    emitBytes(compiler, 1, OP_BITWISE_XOR);
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

static void block(Compiler *compiler) {
  while (!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF)) {
    declaration(compiler);
  }

  consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

// Define: when a variable is available and ready to use, after it's been
// declared.
//
// Defining a variable means putting its name into a ObjString, but storing the
// index in the VM operation with its index in the Chunk's constants table, as
// otherwise would be too expensive.
static void defineVariable(Compiler *compiler, ConstantIndex variable,
                           bool isConstant) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%sdefineVariable()\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent));
#endif

  // If we're in local scope, there's no code to emit at runtime.
  // VM will have the new value to assign to variable on the top of the stack.
  //
  // e.g.
  //
  // var a = 1+2;
  // var b = 4;
  //
  // Bytecode: [OP_CONSTANT(1), OP_CONSTANT(2), OP_ADD, OP_CONSTANT(4)]
  // Stack: [1], [1, 2], [3 (var a)], [3 (var a), 4 (var b)]
  if (compiler->scopeDepth > 0) {
    // If we're declaring a local variable, we could meet a situation where it's
    // used in the expression, but referring to the one of the previous scope.
    // This is not correct, so we need to prevent it, marking it temporarily
    // "disabled", and "activating" it later.
    //
    // e.g.
    // var a = 5 * a + 2;
    markInitialized(compiler);
    return;
  }

  emitConstantIndex(compiler, variable, OP_DEFINE_GLOBAL,
                    OP_DEFINE_GLOBAL_LONG);

  // If it's a constant, we should also add it there for runtime check.
  // TODO: Is there maybe a better/more efficient way?
  if (isConstant) {
    Chunk *chunk = compiler->currentChunk;
    int index = variable.isLong
                    ? (variable.bytes[0] << 16) | (variable.bytes[1] << 8) |
                          variable.bytes[2]
                    : variable.bytes[0];
    ObjString *name = AS_STRING(chunk->constants.values[index]);
    // Just store a dummy nil value to keep it.
    tableSet(&compiler->memoryManager->constants, name, NIL_VAL);
  }
}

// Variable declaration, parsing it and storing in constant's table (see
// defineVariable() for the details.
static void varDeclaration(Compiler *compiler, bool isConstant) {
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  printf("%svarDeclaration(constant=%d)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent), isConstant);
#endif

  ConstantIndex global =
      parseVariable(compiler, "Expect variable name.", isConstant);

  if (match(compiler, TOKEN_EQUAL)) {
    // Get the variable value.
    expression(compiler);
  } else if (isConstant) {
    error(compiler->parser, "Constants must have an initial value.");
    return;
  } else {
    // Otherwise empty initialization, set nil.
    // Syntactic sugar for `var a = nil`;
    emitBytes(compiler, 1, OP_NIL);
  }

  consume(compiler, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(compiler, global, isConstant);
}

static void expressionStatement(Compiler *compiler) {
  expression(compiler);
  consume(compiler, TOKEN_SEMICOLON, "Expect ';' after value.");
  emitBytes(compiler, 1, OP_POP);
}

static void ifStatement(Compiler *compiler) {
  consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression(compiler);
  consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' at the end of condition.");

  // Backpatching: emit the jump instruction first with a placeholder offset
  // operand. We keep track of where that half-finished instruction is. Next, we
  // compile the then body. Once that's done, we know how far to jump. So we go
  // back and replace that placeholder offset with the real one now that we can
  // calculate it.
  int thenJump = emitJump(compiler, OP_JUMP_IF_FALSE);
  emitBytes(compiler, 1, OP_POP);
  statement(compiler);

  int elseJump = emitJump(compiler, OP_JUMP);
  patchJump(compiler, thenJump);
  emitBytes(compiler, 1, OP_POP);

  if (match(compiler, TOKEN_ELSE))
    statement(compiler);

  patchJump(compiler, elseJump);
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
    case TOKEN_CONST:
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
    varDeclaration(compiler, false);
  } else if (match(compiler, TOKEN_CONST)) {
    varDeclaration(compiler, true);
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
  } else if (match(compiler, TOKEN_IF)) {
    ifStatement(compiler);
  } else if (match(compiler, TOKEN_LEFT_BRACE)) {
    beginScope(compiler);
    block(compiler);
    endScope(compiler);
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
  case TOKEN_TILDE:
    emitBytes(compiler, 1, OP_BITWISE_NOT);
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
#ifdef DEBUG_COMPILE_EXECUTION
  debugIndent++;
  char n[name->length + 1];
  snprintf(n, sizeof(n), "%s", name->start);
  n[sizeof(n) + 1] = '\0';
  printf("%snamedVariable(%s)\n",
         strfromnchars(DEBUG_COMPILE_INDENT_CHAR, debugIndent), n);
  debugIndent--;
#endif

  // If it's a global variable will store it's index in the chunk's constant
  // identifiers.
  ConstantIndex cidx;

  // If it a local variable, get the index of the position in the stack instead
  // (-1 otherwise -> global).
  int localIdx = resolveLocal(compiler, name);

  OpCode shortCodeSet, shortCodeGet, longCodeSet, longCodeGet;

  // Check if it's a constant local variable being reassigned.
  bool constReassignment =
      canAssign && localIdx != -1 && compiler->locals[localIdx].isConst;

  // Global variable case.
  if (localIdx == -1) {
    cidx = identifierConstant(compiler, name);

    shortCodeGet = OP_GET_GLOBAL;
    longCodeGet = OP_GET_GLOBAL_LONG;
    shortCodeSet = OP_SET_GLOBAL;
    longCodeSet = OP_SET_GLOBAL_LONG;
  } else
  // Local variable case.
  {
    // Trick to avoid having a lot branches and duplication in the code.
    cidx.bytes[0] = localIdx;
    cidx.isLong = false;

    shortCodeGet = OP_GET_LOCAL;
    longCodeGet = OP_GET_LOCAL_LONG;
    shortCodeSet = OP_SET_LOCAL;
    longCodeSet = OP_SET_LOCAL_LONG;
  }

  // If we are on an assignment token, this is a setter, so we consume first.
  if (canAssign && match(compiler, TOKEN_EQUAL)) {
    if (constReassignment)
      goto reassignmentError;
    expression(compiler);
    emitConstantIndex(compiler, cidx, shortCodeSet, longCodeSet);
  } else if (canAssign && match(compiler, TOKEN_PLUS_EQUAL)) {
    if (constReassignment)
      goto reassignmentError;
    emitConstantIndex(compiler, cidx, shortCodeGet, longCodeGet);
    expression(compiler);
    emitBytes(compiler, 1, OP_ADD);
    emitConstantIndex(compiler, cidx, shortCodeSet, longCodeSet);
  } else if (canAssign && match(compiler, TOKEN_MINUS_EQUAL)) {
    if (constReassignment)
      goto reassignmentError;
    emitConstantIndex(compiler, cidx, shortCodeGet, longCodeGet);
    expression(compiler);
    emitBytes(compiler, 1, OP_SUBTRACT);
    emitConstantIndex(compiler, cidx, shortCodeSet, longCodeSet);
  } else if (canAssign && match(compiler, TOKEN_STAR_EQUAL)) {
    if (constReassignment)
      goto reassignmentError;
    emitConstantIndex(compiler, cidx, shortCodeGet, longCodeGet);
    expression(compiler);
    emitBytes(compiler, 1, OP_MULTIPLY);
    emitConstantIndex(compiler, cidx, shortCodeSet, longCodeSet);
  } else if (canAssign && match(compiler, TOKEN_SLASH_EQUAL)) {
    if (constReassignment)
      goto reassignmentError;
    emitConstantIndex(compiler, cidx, shortCodeGet, longCodeGet);
    expression(compiler);
    emitBytes(compiler, 1, OP_DIVIDE);
    emitConstantIndex(compiler, cidx, shortCodeSet, longCodeSet);
  } else
  // Otherwise we are on a getter, so we just emit bytecode for that.
  {
    emitConstantIndex(compiler, cidx, shortCodeGet, longCodeGet);
  }

  return;

reassignmentError:
  error(compiler->parser, "Cannot reassign to constant variable.");
  return;
}

// Variable read access (e.g. to read its value in an expression).
static void variable(Compiler *compiler, bool canAssign) {
  namedVariable(compiler, &compiler->parser->prev, canAssign);
}

static void postfix(Compiler *compiler, bool canAssign) {
  UNUSED(canAssign);
  Chunk *currChunk = compiler->currentChunk;

  // At this point, the variable value is already on the stack
  // Due to the GET_LOCAL/GLOBAL(_LONG) emitted by the variable prefix function

  // Check that we have at least compiled the variable operator.
  if (currChunk->count < 2) {
    error(compiler->parser, "Invalid target for postfix operator");
    return;
  }

  // Check if the previous op was a variable load
  // 1. First try looking back 2 bytes (GET_GLOBAL/LOCAL case, 1 operand byte +
  // 1 index byte)
  uint8_t lastOp = currChunk->code[currChunk->count - 2];

  // 2. If not a normal GET_LOCAL/GLOBAL, try looking back 4 bytes
  // (GET_GLOBAL_LONG, 1 operand byte + 3 index bytes)
  if (lastOp != OP_GET_LOCAL && lastOp != OP_GET_GLOBAL) {
    lastOp = currChunk->code[currChunk->count - 4];
  }

  if (lastOp != OP_GET_GLOBAL && lastOp != OP_GET_GLOBAL_LONG &&
      lastOp != OP_GET_LOCAL) {
    error(compiler->parser, "Can only apply postfix operators to a variable");
    return;
  }

  int localIdx = -1;
  ConstantIndex varIndex;

  if (lastOp == OP_GET_LOCAL) {
    // The local variable index must be the one right after the operand
    // OP_GET_LOCAL.
    localIdx = currChunk->code[currChunk->count - 1];
  } else if (lastOp == OP_GET_GLOBAL) {
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
  if (localIdx != -1) {
    emitBytes(compiler, 2, OP_SET_LOCAL, (uint8_t)localIdx);
  } else {
    emitConstantIndex(compiler, varIndex, OP_SET_GLOBAL, OP_SET_GLOBAL_LONG);
  }

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
  endCompiler(compiler);

#ifdef DEBUG_COMPILE_EXECUTION
  printf("\n======== compile end() ========\n\n");
#endif

  return !compiler->parser->hadError;
}
