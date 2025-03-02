#include "repl.h"
#include "common.h"
#include "vm.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef struct {
  char entries[REPL_HISTORY_MAX][REPL_LINE_MAX];
  int count;
  int current;
} History;

typedef struct {
  char content[REPL_LINE_MAX];
  int position;
  int length;
} InputLine;

typedef struct {
  struct termios original;
  History history;
  InputLine line;
  VM *vm;
} REPLState;

static void restore_terminal(struct termios *original) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, original);
}

static void configure_terminal(REPLState *state) {
  tcgetattr(STDIN_FILENO, &state->original);

  struct termios raw = state->original;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  atexit((void (*)(void))restore_terminal);
}

static void clear_line(InputLine *line) {
  memset(line->content, 0, REPL_LINE_MAX);
  line->position = 0;
  line->length = 0;
}

static void render_line(const InputLine *line) {
  // First clear the entire line
  write(STDOUT_FILENO, "\r\x1b[K", 4);

  // Write prompt and content
  write(STDOUT_FILENO, "> ", 2);
  if (line->length > 0) {
    write(STDOUT_FILENO, line->content, line->length);
  }

  // Position cursor correctly
  if (line->position < line->length) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\r\x1b[%dC", line->position + 2); // +2 for "> "
    write(STDOUT_FILENO, buf, strlen(buf));
  }
}

static void history_add(History *history, const char *line) {
  if (strlen(line) == 0)
    return;

  if (history->count < REPL_HISTORY_MAX) {
    strncpy(history->entries[history->count], line, REPL_LINE_MAX - 1);
    history->entries[history->count][REPL_LINE_MAX - 1] = '\0';
    history->count++;
  } else {
    memmove(history->entries[0], history->entries[1],
            sizeof(history->entries[0]) * (REPL_HISTORY_MAX - 1));
    strncpy(history->entries[REPL_HISTORY_MAX - 1], line, REPL_LINE_MAX - 1);
    history->entries[REPL_HISTORY_MAX - 1][REPL_LINE_MAX - 1] = '\0';
  }
  history->current = history->count;
}

static void handle_history_navigation(REPLState *state, bool up) {
  History *history = &state->history;
  InputLine *line = &state->line;

  int new_current = up ? history->current - 1 : history->current + 1;

  if (up && new_current >= 0) {
    history->current = new_current;
    clear_line(line);
    strncpy(line->content, history->entries[history->current],
            REPL_LINE_MAX - 1);
    line->content[REPL_LINE_MAX - 1] = '\0';
    line->length = strlen(line->content);
    line->position = line->length;
  } else if (!up && new_current <= history->count) {
    history->current = new_current;
    clear_line(line);

    if (new_current < history->count) {
      strncpy(line->content, history->entries[history->current],
              REPL_LINE_MAX - 1);
      line->content[REPL_LINE_MAX - 1] = '\0';
      line->length = strlen(line->content);
      line->position = line->length;
    }
  }
  render_line(line);
}

static bool handle_escape_sequence(REPLState *state) {
  char seq[2];
  if (read(STDIN_FILENO, &seq[0], 1) != 1 ||
      read(STDIN_FILENO, &seq[1], 1) != 1) {
    return false;
  }

  if (seq[0] != '[')
    return false;

  InputLine *line = &state->line;
  switch (seq[1]) {
  case ARROW_UP:
  case ARROW_DOWN:
    handle_history_navigation(state, seq[1] == 'A');
    break;
  case ARROW_RIGHT:
    if (line->position < line->length) {
      line->position++;
      render_line(line);
    }
    break;
  case ARROW_LEFT:
    if (line->position > 0) {
      line->position--;
      render_line(line);
    }
    break;
  }
  return true;
}

static void handle_backspace(REPLState *state) {
  InputLine *line = &state->line;
  if (line->position > 0) {
    memmove(&line->content[line->position - 1], &line->content[line->position],
            line->length - line->position + 1);
    line->position--;
    line->length--;
    render_line(line);
  }
}

static void handle_regular_input(REPLState *state, char c) {
  InputLine *line = &state->line;
  if (line->length < REPL_LINE_MAX - 1) {
    if (line->position < line->length) {
      memmove(&line->content[line->position + 1],
              &line->content[line->position], line->length - line->position);
    }
    line->content[line->position] = c;
    line->position++;
    line->length++;
    line->content[line->length] = '\0';
    render_line(line);
  }
}

void repl() {
  REPLState state = {0};
  state.vm = initVM();

  printf("\nWelcome to nrk v%s.\n", NRK_VERSION);
  configure_terminal(&state);

  while (true) {
    printf("> ");
    fflush(stdout);

    clear_line(&state.line);

    while (true) {
      char c;
      if (read(STDIN_FILENO, &c, 1) != 1)
        continue;

      if (c == CTRL_D) {
        printf("\n");
        goto cleanup;
      }

      if (c == ESC) {
        if (handle_escape_sequence(&state))
          continue;
        break;
      }

      if (c == '\n') {
        write(STDOUT_FILENO, "\n", 1);
        break;
      }

      if (c == BACKSPACE || c == '\b') {
        handle_backspace(&state);
        continue;
      }

      handle_regular_input(&state, c);
    }

    if (state.line.length > 0) {
      // Ensure null termination before processing
      state.line.content[state.line.length] = '\0';
      history_add(&state.history, state.line.content);
      char *source = stripstring(state.line.content);
      interpret(state.vm, source);
    }
  }

cleanup:
  restore_terminal(&state.original);
  freeVM(state.vm);
}
