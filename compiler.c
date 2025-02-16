#include "compiler.h"
#include "common.h"
#include "scanner.h"
#include "vm.h"
#include <stdio.h>

void compile(VM *vm, const char *source) {
  Scanner *scanner = initScanner(source);

  int line = -1;
  for (;;) {
    Token token = scanToken(scanner);
    if (token.line != line) {
      printf("%04d ", token.line);
    } else {
      printf("    | ");
    }

    // %.*s allows to print exactly token.length chars as we don't have a \0
    // here.
    printf("%-20s \"%.*s\"\n", tokenTypeToString(token.type), token.length,
           token.start);

    if (token.type == TOKEN_ERROR)
      break;
    if (token.type == TOKEN_EOF)
      break;
  }

  freeScanner(scanner);
}
