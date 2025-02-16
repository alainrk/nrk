.PHONY: all clean run build

# Compiler settings
CC = clang
CFLAGS = -Wall -Wpointer-sign -Wextra -g

# Project files
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
TARGET = main

# Default target
all: $(TARGET)

# Link object files into binary
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Generate compilation database using build
build:
	bear -- make all

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET) compile_commands.json

# Run the program
run: $(TARGET)
	./$(TARGET)
