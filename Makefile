CC = clang
CFLAGS = -std=c99 -Wall -Wextra -Werror -g
LDFLAGS = -lreadline
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Create directories if they don't exist
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Object files (replace src/ with obj/ in the path)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
# Main executable
MAIN = $(BIN_DIR)/nrk

.PHONY: all clean

all: $(MAIN)

# Linking rule
$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compilation rule: each .c file to corresponding .o file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Include automatically generated dependencies
-include $(OBJS:.o=.d)

# Rule to generate a dependency file
$(OBJ_DIR)/%.d: $(SRC_DIR)/%.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(OBJ_DIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/* *.o *.d

# Run the program in REPL mode
run: $(MAIN)
	$(MAIN)

# Run a specific test or script
test: $(MAIN)
	$(MAIN) test/test_script.nrk

# Debug with GDB
debug: $(MAIN)
	gdb $(MAIN)
