#ifndef nrk_scanner_h
#define nrk_scanner_h

#include <stdbool.h>

typedef struct {
  const char *start;
  const char *curr;
  int line;
  bool inTemplate;
  int templateNesting;
} Scanner;

typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_MINUS,
  TOKEN_PLUS,
  TOKEN_SEMICOLON,
  TOKEN_SLASH,
  TOKEN_STAR,
  TOKEN_DOLLAR,
  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,
  TOKEN_GREATER_GREATER, // >> (bit shift right)
  TOKEN_LESS_LESS,       // << (bit shift left)
  TOKEN_AMPERSEND,       // & (bit and)
  TOKEN_CARET,           // ^ (bit xor)
  TOKEN_PIPE,            // | (bit or)
  TOKEN_TILDE,           // ~ (bit not)
  TOKEN_PLUS_PLUS,       // ++
  TOKEN_MINUS_MINUS,     // --
  TOKEN_PLUS_EQUAL,      // +=
  TOKEN_MINUS_EQUAL,     // -=
  TOKEN_STAR_EQUAL,      // *=
  TOKEN_SLASH_EQUAL,     // /=
  // Template strings.
  TOKEN_TEMPL_START,        // Opening "`"
  TOKEN_TEMPL_END,          // Closing "`"
  TOKEN_TEMPL_INTERP_START, // Opening "${"
  TOKEN_TEMPL_INTERP_END,   // Closing "}"
  TOKEN_TEMPL_CONTENT,      // Non-expression content
  // Literals.
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NUMBER,
  // Keywords.
  TOKEN_AND,
  TOKEN_CLASS,
  TOKEN_ELSE,
  TOKEN_FALSE,
  TOKEN_FOR,
  TOKEN_FUN,
  TOKEN_IF,
  TOKEN_NIL,
  TOKEN_OR,
  TOKEN_PRINT,
  TOKEN_RETURN,
  TOKEN_SUPER,
  TOKEN_THIS,
  TOKEN_TRUE,
  TOKEN_VAR,
  TOKEN_WHILE,
  // Specials.
  TOKEN_ERROR,
  TOKEN_EOF
} TokenType;

typedef struct {
  TokenType type;
  const char *start;
  int length;
  int line;
} Token;

Scanner *initScanner(const char *source);
void freeScanner(Scanner *scanner);
Token scanToken(Scanner *scanner);
const char *tokenTypeToString(TokenType type);

#endif
