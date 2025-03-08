#include "repl.h"
#include "common.h"
#include "vm.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
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
  // Store current terminal settings for later restoration
  tcgetattr(STDIN_FILENO, &state->original);

  // Create a copy of original settings to modify
  struct termios raw = state->original;
  // Disable canonical mode and echo (process keys immediately, don't display
  // typed chars)
  raw.c_lflag &= ~(ICANON | ECHO);
  // Return read() after at least one byte is available
  raw.c_cc[VMIN] = 1;
  // No timeout for read()
  raw.c_cc[VTIME] = 0;

  // Apply new terminal settings
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  // Register function to restore terminal settings on program exit
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

// Load history from file to history struct
static void history_load_from_file(History *history) {
  FILE *file = fopen(HISTORY_FILE_PATH, "r");
  if (file == NULL) {
    // File doesn't exist or can't be opened, which is fine for first run
    return;
  }

  // Lock the file for reading
  if (flock(fileno(file), LOCK_SH) == -1) {
    fclose(file);
    return;
  }

  // Count lines in file to determine how many to load
  long lines_count = 0;
  char count_buffer[REPL_LINE_MAX];
  while (fgets(count_buffer, sizeof(count_buffer), file) != NULL) {
    lines_count++;
  }

  // Reset file position to beginning
  rewind(file);

  // If file has more entries than REPL_HISTORY_MAX, skip early entries
  if (lines_count > REPL_HISTORY_MAX) {
    long skip_lines = lines_count - REPL_HISTORY_MAX;
    for (long i = 0; i < skip_lines; i++) {
      if (fgets(count_buffer, sizeof(count_buffer), file) == NULL) {
        break;
      }
    }
  }

  // Load entries into history
  history->count = 0;
  while (history->count < REPL_HISTORY_MAX &&
         fgets(history->entries[history->count], REPL_LINE_MAX, file) != NULL) {
    // Remove trailing newline if present
    size_t len = strlen(history->entries[history->count]);
    if (len > 0 && history->entries[history->count][len - 1] == '\n') {
      history->entries[history->count][len - 1] = '\0';
    }
    history->count++;
  }

  // Set current position to end of history
  history->current = history->count;

  // Unlock and close the file
  flock(fileno(file), LOCK_UN);
  fclose(file);
}

// Append a line to history file
static void history_append_to_file(const char *line) {
  if (strlen(line) == 0) {
    return;
  }

  FILE *file = fopen(HISTORY_FILE_PATH, "a");
  if (file == NULL) {
    return;
  }

  // Lock the file for writing
  if (flock(fileno(file), LOCK_EX) == -1) {
    fclose(file);
    return;
  }

  // Append the line with a newline
  fprintf(file, "%s\n", line);

  // Unlock and close the file
  flock(fileno(file), LOCK_UN);
  fclose(file);
}

static void history_add(History *history, const char *line) {
  if (strlen(line) == 0)
    return;

  // Append to file first
  history_append_to_file(line);

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

// Function to navigate through command history using up/down keys
static void handle_history_navigation(REPLState *state, bool up) {
  History *history = &state->history;
  InputLine *line = &state->line;

  // Calculate new history position based on direction
  int new_current = up ? history->current - 1 : history->current + 1;

  // Upwards
  if (up && new_current >= 0) {
    history->current = new_current;
    clear_line(line);

    // Copy the historical command to current line, ensuring we don't overflow
    strncpy(line->content, history->entries[history->current],
            REPL_LINE_MAX - 1);

    // Ensure null-termination in case the history entry fills the buffer
    line->content[REPL_LINE_MAX - 1] = '\0';

    // Update line length and cursor position
    line->length = strlen(line->content);
    line->position = line->length;
  }
  // Downwards
  else if (!up && new_current <= history->count) {
    history->current = new_current;
    clear_line(line);

    // Only copy content if we're not at the newest position (empty line)
    if (new_current < history->count) {
      // Copy the historical command to current line, ensuring we don't overflow
      strncpy(line->content, history->entries[history->current],
              REPL_LINE_MAX - 1);

      // Ensure null-termination in case the history entry fills the buffer
      line->content[REPL_LINE_MAX - 1] = '\0';

      // Update line length and cursor position
      line->length = strlen(line->content);
      line->position = line->length;
    }
  }

  // Render the updated line to the screen
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

  // Load history from file
  history_load_from_file(&state.history);

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
