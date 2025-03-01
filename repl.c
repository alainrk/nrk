#include "common.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define HISTORY_MAX 50
#define LINE_MAX 1024

static struct {
  char entries[HISTORY_MAX][LINE_MAX];
  int count;
  int current;
} history;

static struct termios orig_termios;

static void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;

  // Turn off ECHO and ICANON (canonical mode)
  raw.c_lflag &= ~(ICANON | ECHO);

  // Set read timeout
  raw.c_cc[VMIN] = 1;  // Wait for at least one character
  raw.c_cc[VTIME] = 0; // No timeout

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void add_history(const char *line) {
  if (history.count < HISTORY_MAX) {
    strcpy(history.entries[history.count++], line);
  } else {
    // Shift everything down to make room
    memmove(history.entries[0], history.entries[1],
            sizeof(history.entries[0]) * (HISTORY_MAX - 1));
    strcpy(history.entries[HISTORY_MAX - 1], line);
  }
  history.current = history.count;
}

void repl() {
  VM *vm = initVM();
  printf("\nWelcome to nrk v0.0.1.\n");

  enable_raw_mode();
  memset(&history, 0, sizeof(history));

  char line[LINE_MAX];

  for (;;) {
    printf("> ");
    fflush(stdout);

    int pos = 0;
    memset(line, 0, sizeof(line));

    while (true) {
      char c;
      if (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 4) { // Ctrl+D
          printf("\n");
          goto out;
        }

        if (c == 27) { // ESC sequence
          char seq[2];
          if (read(STDIN_FILENO, &seq[0], 1) != 1)
            break;
          if (read(STDIN_FILENO, &seq[1], 1) != 1)
            break;

          if (seq[0] == '[') {
            if (seq[1] == 'A') { // Up arrow
              if (history.current > 0) {
                // Clear current line
                while (pos > 0) {
                  printf("\b \b");
                  pos--;
                }

                history.current--;
                strcpy(line, history.entries[history.current]);
                printf("%s", line);
                fflush(stdout); // Force display
                pos = strlen(line);
              }
            } else if (seq[1] == 'B') { // Down arrow
              if (history.current < history.count) {
                // Clear current line
                while (pos > 0) {
                  printf("\b \b");
                  pos--;
                }

                history.current++;
                if (history.current < history.count) {
                  strcpy(line, history.entries[history.current]);
                  printf("%s", line);
                  fflush(stdout); // Force display
                  pos = strlen(line);
                } else {
                  // Clear the line when pressing down at the end of history
                  line[0] = '\0';
                }
              }
            }
          }
        } else if (c == '\n') {
          printf("\n");
          break;
        } else if (c == 127 ||
                   c == '\b') { // Backspace (127 is DEL, '\b' is Ctrl+H)
          if (pos > 0) {
            line[--pos] = '\0';
            // Move cursor back, print space, move cursor back again
            printf("\b \b");
            // Make sure it's displayed immediately
            fflush(stdout);
          }
        } else if (pos < sizeof(line) - 1) {
          line[pos++] = c;
          printf("%c", c); // Make sure to echo the character
          fflush(stdout);  // Flush to ensure immediate display
        }
      }
    }

    if (strlen(line) > 0) {
      add_history(line);
      char *source = stripstring(line);
      interpret(vm, source);
    }
  }

out:
  disable_raw_mode();
  freeVM(vm);
}
