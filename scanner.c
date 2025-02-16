#include "scanner.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *tokenTypeToString(TokenType type) {
  switch (type) {
  case TOKEN_LEFT_PAREN:
    return "TOKEN_LEFT_PAREN";
  case TOKEN_RIGHT_PAREN:
    return "TOKEN_RIGHT_PAREN";
  case TOKEN_LEFT_BRACE:
    return "TOKEN_LEFT_BRACE";
  case TOKEN_RIGHT_BRACE:
    return "TOKEN_RIGHT_BRACE";
  case TOKEN_COMMA:
    return "TOKEN_COMMA";
  case TOKEN_DOT:
    return "TOKEN_DOT";
  case TOKEN_MINUS:
    return "TOKEN_MINUS";
  case TOKEN_PLUS:
    return "TOKEN_PLUS";
  case TOKEN_SEMICOLON:
    return "TOKEN_SEMICOLON";
  case TOKEN_SLASH:
    return "TOKEN_SLASH";
  case TOKEN_STAR:
    return "TOKEN_STAR";
  case TOKEN_BANG:
    return "TOKEN_BANG";
  case TOKEN_BANG_EQUAL:
    return "TOKEN_BANG_EQUAL";
  case TOKEN_EQUAL:
    return "TOKEN_EQUAL";
  case TOKEN_EQUAL_EQUAL:
    return "TOKEN_EQUAL_EQUAL";
  case TOKEN_GREATER:
    return "TOKEN_GREATER";
  case TOKEN_GREATER_EQUAL:
    return "TOKEN_GREATER_EQUAL";
  case TOKEN_LESS:
    return "TOKEN_LESS";
  case TOKEN_LESS_EQUAL:
    return "TOKEN_LESS_EQUAL";
  case TOKEN_IDENTIFIER:
    return "TOKEN_IDENTIFIER";
  case TOKEN_STRING:
    return "TOKEN_STRING";
  case TOKEN_NUMBER:
    return "TOKEN_NUMBER";
  case TOKEN_AND:
    return "TOKEN_AND";
  case TOKEN_CLASS:
    return "TOKEN_CLASS";
  case TOKEN_ELSE:
    return "TOKEN_ELSE";
  case TOKEN_FALSE:
    return "TOKEN_FALSE";
  case TOKEN_FOR:
    return "TOKEN_FOR";
  case TOKEN_FUN:
    return "TOKEN_FUN";
  case TOKEN_IF:
    return "TOKEN_IF";
  case TOKEN_NIL:
    return "TOKEN_NIL";
  case TOKEN_OR:
    return "TOKEN_OR";
  case TOKEN_PRINT:
    return "TOKEN_PRINT";
  case TOKEN_RETURN:
    return "TOKEN_RETURN";
  case TOKEN_SUPER:
    return "TOKEN_SUPER";
  case TOKEN_THIS:
    return "TOKEN_THIS";
  case TOKEN_TRUE:
    return "TOKEN_TRUE";
  case TOKEN_VAR:
    return "TOKEN_VAR";
  case TOKEN_WHILE:
    return "TOKEN_WHILE";
  case TOKEN_ERROR:
    return "TOKEN_ERROR";
  case TOKEN_EOF:
    return "TOKEN_EOF";
  default: {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "UNKNOWN_TOKEN(%d)", type);
    return buffer;
  }
  }
}
Scanner *initScanner(const char *source) {
  Scanner *scanner = (Scanner *)malloc(sizeof(Scanner));

  scanner->start = source;
  scanner->curr = source;
  scanner->line = 1;

  return scanner;
}

void freeScanner(Scanner *scanner) { free(scanner); }

static bool isAtEnd(Scanner *scanner) { return *scanner->curr == '\0'; }

// Returns the current char without consuming it.
static char peek(Scanner *scanner) { return *scanner->curr; }

// Returns the next char without consuming it.
// Returns \0 if we're at the end already.
static char peekNext(Scanner *scanner) {
  if (isAtEnd(scanner)) {
    return '\0';
  }
  return scanner->curr[1];
}

static char advance(Scanner *scanner) {
  scanner->curr++;
  return scanner->curr[-1];
}

static void skipWhitespaceAndComments(Scanner *scanner) {
  for (;;) {
    char c = peek(scanner);

    switch (c) {
    case ' ':
    case '\t':
    case '\r':
      advance(scanner);
      break;
    case '\n':
      scanner->line++;
      advance(scanner);
      break;
    case '/':
      if (peekNext(scanner) == '/') {
        // Consume till the end of the line or file.
        // Note tha we only peek the possible newline, so it gets taken at the
        // next loop so to increment the current line.
        while (peek(scanner) != '\n' && !isAtEnd(scanner))
          advance(scanner);
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
};

static bool match(Scanner *scanner, char expected) {
  if (isAtEnd(scanner))
    return false;

  if (*scanner->curr != expected)
    return false;

  scanner->curr++;

  return true;
}

static bool isDigit(char c) { return (c >= '0' && c <= '9'); }

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static Token errorToken(Scanner *scanner, char *message) {
  Token token;

  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner->line;

  return token;
}

Token makeToken(Scanner *scanner, TokenType t) {
  Token token;

  token.type = t;
  token.start = scanner->start;
  token.length = (int)(scanner->curr - scanner->start);
  token.line = scanner->line;

  return token;
}

static TokenType checkKeyword(Scanner *scanner, int start, int length,
                              const char *rest, TokenType type) {
  // Check if the rest of the string is the same (and so first check that the
  // length is).
  if (scanner->curr - scanner->start == start + length &&
      memcmp(scanner->start + start, rest, length) == 0)
    return type;

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner *scanner) {
  switch (scanner->start[0]) {
  case 'a':
    return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND);
  case 'c':
    return checkKeyword(scanner, 1, 4, "lass", TOKEN_CLASS);
  case 'e':
    return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
  case 'f':
    if (scanner->curr - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'a':
        return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
      case 'u':
        return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
      }
    }
    break;
  case 'i':
    return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
  case 'n':
    return checkKeyword(scanner, 1, 2, "il", TOKEN_NIL);
  case 'o':
    return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
  case 'p':
    return checkKeyword(scanner, 1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
  case 's':
    return checkKeyword(scanner, 1, 4, "uper", TOKEN_SUPER);
  case 't':
    if (scanner->curr - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'h':
        return checkKeyword(scanner, 2, 2, "is", TOKEN_THIS);
      case 'r':
        return checkKeyword(scanner, 2, 2, "ue", TOKEN_TRUE);
      }
    }
    break;
  case 'v':
    return checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR);
  case 'w':
    return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

// Returns an identifier, it must start with an alpha and can proceed with
// alphanumeric.
static Token identifier(Scanner *scanner) {
  while (isAlpha(peek(scanner)) || isDigit(peek(scanner)))
    advance(scanner);
  return makeToken(scanner, identifierType(scanner));
}

Token string(Scanner *scanner) {
  while (peek(scanner) != '"' && !isAtEnd(scanner)) {
    if (peek(scanner) == '\n')
      scanner->line++;
    advance(scanner);
  }

  if (isAtEnd(scanner)) {
    return errorToken(scanner, "Unterminated string");
  }

  advance(scanner);
  return makeToken(scanner, TOKEN_STRING);
}

Token number(Scanner *scanner) {
  while (isDigit(peek(scanner)))
    advance(scanner);

  if (peek(scanner) == '.' && peekNext(scanner))
    advance(scanner);

  while (isDigit(peek(scanner)))
    advance(scanner);

  return makeToken(scanner, TOKEN_NUMBER);
}

Token scanToken(Scanner *scanner) {
  skipWhitespaceAndComments(scanner);

  // Set the start of the current token, so in makeToken() it can calculate the
  // length.
  scanner->start = scanner->curr;

  if (isAtEnd(scanner)) {
    return makeToken(scanner, TOKEN_EOF);
  }

  char c = advance(scanner);

  // printf("Char: %c\n", c);

  if (isAlpha(c)) {
    return identifier(scanner);
  }

  if (isDigit(c)) {
    return number(scanner);
  }

  switch (c) {
  case '(':
    return makeToken(scanner, TOKEN_LEFT_PAREN);
  case ')':
    return makeToken(scanner, TOKEN_RIGHT_PAREN);
  case '{':
    return makeToken(scanner, TOKEN_LEFT_BRACE);
  case '}':
    return makeToken(scanner, TOKEN_RIGHT_BRACE);
  case ';':
    return makeToken(scanner, TOKEN_SEMICOLON);
  case ',':
    return makeToken(scanner, TOKEN_COMMA);
  case '.':
    return makeToken(scanner, TOKEN_DOT);
  case '-':
    return makeToken(scanner, TOKEN_MINUS);
  case '+':
    return makeToken(scanner, TOKEN_PLUS);
  case '/':
    return makeToken(scanner, TOKEN_SLASH);
  case '*':
    return makeToken(scanner, TOKEN_STAR);
  case '!':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '"':
    return string(scanner);
  }

  printf("Unexpected character \"%c\".\n", c);
  return errorToken(scanner, "Unexpected character");
}
