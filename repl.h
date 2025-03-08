#ifndef nrk_repl_h
#define nrk_repl_h

#define REPL_HISTORY_MAX (1 << 8)
#define REPL_LINE_MAX (1 << 10)

#define HISTORY_FILE_PATH "/tmp/nrklang_repl_history_log"

#define ESC '\x1b'
#define CTRL_D 4
#define BACKSPACE 127
#define ARROW_UP 'A'
#define ARROW_DOWN 'B'
#define ARROW_RIGHT 'C'
#define ARROW_LEFT 'D'

void repl();

#endif
