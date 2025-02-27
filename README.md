# nrk Programming Language

A bytecode interpreter implementation inspired by Bob Nystrom's ["Crafting Interpreters"](https://craftinginterpreters.com/).

## Overview

`nrk` is a small programming language with a bytecode virtual machine written in C. This project follows the implementation described in the book "Crafting Interpreters", with some custom modifications and enhancements.

## Features

- **Bytecode Virtual Machine**: Executes instructions efficiently with a stack-based architecture
- **C99 Implementation**: Written in standard C99 for maximum portability
- **REPL**: Interactive Read-Eval-Print Loop for quick testing and experimentation
- **File Execution**: Run `nrk` script files directly
- **Expression Evaluation**: Currently supports arithmetic expressions

## Current Implementation

The language currently supports:

- Basic arithmetic operations (`+`, `-`, `*`, `/`)
- Numeric literals
- Unary negation
- Parenthesized expressions
- Extended constant pool (via `OP_CONSTANT_LONG` for more than 256 constants)

## Project Structure

- **VM**: Stack-based virtual machine that executes bytecode
- **Compiler**: Turns source code into bytecode
- **Scanner**: Tokenizes source code for the compiler
- **Chunk**: Represents bytecode and related metadata
- **Value**: Represents runtime values (currently double-precision floating point)
- **Debug**: Tools for inspecting bytecode and execution

## Building and Running

### Prerequisites

- C compiler (clang recommended)
- Make

### Build

```bash
make
```

### Run the REPL

```bash
./main
```

### Run a File

```bash
./main path/to/script.nrk
```

## Example Code

```
1 + 2 * 3
```

Will produce:

```
7
```

## Development Status

`nrk` is currently in early development. The focus has been on implementing the core bytecode VM, scanner, and compiler for arithmetic expressions. Future plans include:

- Variables and assignment
- String support and operations
- Control flow (if/else, loops)
- Functions and closures
- Classes and inheritance

## Debugging Features

The interpreter includes several debugging features that can be enabled by uncommenting the corresponding define statements in `common.h`:

- `DEBUG_PRINT_CODE`: Disassembles and prints bytecode after compilation
- `DEBUG_TRACE_EXECUTION`: Traces VM execution step by step, showing stack state
- `DEBUG_COMPILE_EXECUTION`: Provides detailed compilation process logs
