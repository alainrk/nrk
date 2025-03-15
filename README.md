# NRK Programming Language

**nrk** (`/ˈanɑːrki/`) is a small programming language with a bytecode interpreter implementation written entirely in C99.
The language is inspired by Bob Nystrom's ["Crafting Interpreters"](https://craftinginterpreters.com/) with modifications and enhancements.

## Features

- **Bytecode Virtual Machine**: Executes instructions efficiently with a stack-based architecture
- **C99 Implementation**: Written in standard C99 for maximum portability
- **REPL**: Interactive Read-Eval-Print Loop with history persistence and line editing
- **File Execution**: Run `nrk` script files directly
- **Expression Evaluation**: Currently supports arithmetic expressions and string operations

## Current Implementation

The language currently supports:

- Basic arithmetic operations (`+`, `-`, `*`, `/`)
- Comparison operations (`==`, `!=`, `>`, `<`, `>=`, `<=`)
- Numeric literals
- String literals and concatenation
- Unary negation
- Boolean values (`true`, `false`)
- Logical negation (`!`)
- `nil` value
- Type checking for operations
- Print statements (`print "Hello, World!";`)

Additional custom features implemented in `nrk`:

- Template (and nested!) strings (`` `This is a ${${"nested"} str}} with ${1+1} levels of nesting.` ``)
- String operations (concatenation with `+`)
- String interning for efficient string storage and comparison
- Extended constant pool (via `OP_CONSTANT_LONG` for more than 256 constants)
- Tagged value representation (numbers, booleans, nil, strings)
- Runtime type checking
- Advanced REPL with arrow key navigation, command history, and persistent history across sessions
- Memory management with garbage collection foundations
- Hash table implementation for string interning

## Project Structure

- **VM**: Stack-based virtual machine that executes bytecode
- **Compiler**: Turns source code into bytecode
- **Scanner**: Tokenizes source code for the compiler
- **Chunk**: Represents bytecode and related metadata
- **Value**: Represents runtime values (numbers, booleans, nil, strings)
- **Object**: Manages heap-allocated objects like strings
- **Memory**: Handles memory allocation, reallocation, and garbage collection
- **Debug**: Tools for inspecting bytecode and execution
- **REPL**: Interactive environment with history, line editing, and history persistence
- **Table**: Hash table implementation for string interning and future variable lookup

## Building and Running

### Prerequisites

- C compiler (clang recommended)
- Make

### Build

```sh
make all
```

### Run the REPL

```sh
./main
```

### Run a File

```sh
./main path/to/script.nrk
```

## Example Code

Calculations:

```c
1 + 2 * (3 + 4) / 5 // 3.8
```

Boolean operations:

```go
!true // false
!!0   // false (cast to boolean)
!nil  // true (falsey values = 0, false, nil)
```

Comparison operations:

```go
1 == 1     // true
2 != 2     // false
3 > 2      // true
4 < 3      // false
5 >= 5     // true
6 <= 7     // true
```

String operations:

```go
"Hello, " + "world!" // "Hello, world!"
```

Print statements:

```go
print "Hello, world!";
print 2 + 2;
```

## Development Status

`nrk` is currently in early development with version v0.0.1. Recent implementations include:

- String literals and string operations
- String interning with hash table implementation
- Memory management infrastructure with Flexible Array Members (FAM)
- REPL with persistent history across sessions
- Foundation for garbage collection
- Print statements

Future plans include:

- Variables and assignment
- Control flow (if/else, loops)
- Functions and closures
- Classes and inheritance

## Debugging Features

The interpreter includes several debugging features that can be enabled by uncommenting the corresponding define statements in `common.h`:

- `DEBUG_PRINT_CODE`: Disassembles and prints bytecode after compilation
- `DEBUG_TRACE_EXECUTION`: Traces VM execution step by step, showing stack state
- `DEBUG_SCAN_EXECUTION`: Shows detailed scanning process information
- `DEBUG_COMPILE_EXECUTION`: Provides detailed compilation process logs

## TODO

- Extend table's support to other immutable objects other than strings as keys.
- In compiler avoid multiple pointer indirections with compiler->parser/scanner if used in critical paths and just bring it to a var inside the function.
