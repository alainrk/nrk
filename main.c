#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "test.h"
#include "vm.h"

static void repl() {
  VM *vm = initVM();

  printf("\nWelcome to nrk v0.0.1.\n");

  for (;;) {
    printf("> ");
    char line[1024];
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    char *source = stripstring(line);
    if (strlen(source) == 1) {
      continue;
    }
    interpret(vm, source);
  }

  freeVM(vm);
}

static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file %s\n", path);
    exit(74);
  }

  // Get the file length
  fseek(file, 0L, SEEK_END);
  size_t fsize = ftell(file);
  rewind(file);

  // +1 space for \0
  char *buffer = (char *)malloc(fsize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }

  size_t nbytes = fread(buffer, sizeof(char), fsize, file);
  if (nbytes < fsize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[nbytes] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(const char *path) {
  VM *vm = initVM();

  char *source = readFile(path);

  printf("=== src ===\n%s\n===========\n", source);

  InterpretResult res = interpret(vm, source);
  free(source);

  if (res == INTERPRET_COMPILE_ERROR)
    exit(65);
  if (res == INTERPRET_RUNTIME_ERROR)
    exit(70);

  freeVM(vm);
}

void testInterpreter(int argc, char **argv) {
  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: nrk [path/file.nrk]\n\n");
    exit(64);
  }
}

int main(int argc, char **argv) {
  // testResetStack();
  // testLongConst();
  // testAdd();
  // testArithmetics();
  // testNegate();

  testInterpreter(argc, argv);

  return 0;
}
