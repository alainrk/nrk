# NRK Programming Language

**nrk** (`/ˈanɑːrki/`) is a small programming language with a bytecode interpreter implementation written entirely in C99.
The language is inspired by Bob Nystrom's ["Crafting Interpreters"](https://craftinginterpreters.com/) with modifications and enhancements.

## Doc

- [Features](#features)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Building](#building)
  - [Running](#running)
- [Language Guide](#language-guide)
  - [Basic Syntax](#basic-syntax)
  - [Data Types](#data-types)
  - [Operators](#operators)
  - [Variables](#variables)
  - [Expressions](#expressions)
  - [Statements](#statements)
- [Implementation Details](#implementation-details)
  - [Architecture](#architecture)
  - [Project Structure](#project-structure)
  - [Development Status](#development-status)
  - [Debugging](#debugging)
- [Roadmap](#roadmap)
- [Development Notes](#development-notes)

## Features

- **Bytecode Virtual Machine**: Executes instructions efficiently with a stack-based architecture
- **C99 Implementation**: Written in standard C99 for maximum portability
- **REPL**: Interactive Read-Eval-Print Loop with history persistence and line editing
- **File Execution**: Run `nrk` script files directly
- **Expression Evaluation**: Supports arithmetic expressions, string operations, and variables
- **Type System**: Dynamic typing with runtime type checking
- **Operators**: Arithmetic, comparison, logical, bitwise, and postfix operations
- **String Handling**: String literals, concatenation, interning, and template strings
- **Memory Management**: Efficient memory allocation with garbage collection foundations

## Getting Started

### Prerequisites

- C compiler (clang recommended)
- Make

### Building

```sh
make all
```

### Running

**REPL Mode**:

```sh
./bin/nrk
```

**File Execution**:

```sh
./bin/nrk path/to/script.nrk
```

## Language Guide

### Basic Syntax

NRK uses semicolons to terminate statements:

```go
print "Hello, world!";
var x = 10;
```

### Data Types

NRK currently supports the following primitive types:

- **Numbers**: `42`, `3.14159`
- **Strings**: `"Hello, world!"`
- **Booleans**: `true`, `false`
- **Nil**: `nil` (represents absence of a value)

Template strings are also supported for more dynamic string creation:

```go
`This is a ${1+1} in a template string`  // "This is a 2 in a template string"
`Nested templates: ${`inner ${42}`}`     // "Nested templates: inner 42"
```

### Operators

#### Arithmetic Operators

```go
1 + 2         // Addition: 3
10 - 5        // Subtraction: 5
3 * 4         // Multiplication: 12
10 / 2        // Division: 5
```

#### Bitwise Operators

```go
~5            // Bitwise NOT: -6
8 >> 2        // Bitwise right shift: 2
2 << 3        // Bitwise left shift: 16
12 & 5        // Bitwise AND: 4
12 | 5        // Bitwise OR: 13
12 ^ 5        // Bitwise XOR: 9
```

#### Comparison Operators

```go
1 == 1        // Equal: true
2 != 2        // Not equal: false
3 > 2         // Greater than: true
4 < 3         // Less than: false
5 >= 5        // Greater than or equal: true
6 <= 7        // Less than or equal: true
```

#### Logical Operators

```go
!true         // Logical NOT: false
!!0           // Double NOT: false (0 converts to false first)
!nil          // NOT nil: true (nil is falsey)
```

#### Postfix Operators

```go
var a = 5;
a++           // Increment: a becomes 6
a--           // Decrement: a becomes 5
```

#### String Operations

```go
"Hello, " + "world!"  // Concatenation: "Hello, world!"
```

### Variables

Variables are declared using the `var` keyword:

```go
var a = 5;
var name = "Alice";
var isActive = true;
```

Variables can be modified after declaration:

```go
var counter = 0;
counter = 10;
counter++;           // Now 11
```

Compound assignment is also supported:

```go
var x = 5;
x += 3;              // x = x + 3: x becomes 8
x -= 2;              // x = x - 2: x becomes 6
x *= 2;              // x = x * 2: x becomes 12
x /= 4;              // x = x / 4: x becomes 3
```

### Expressions

NRK supports complex expressions with operator precedence:

```go
1 + 2 * (3 + 4) / 5  // 3.8
```

### Statements

#### Print Statement

```go
print "Hello, world!";
print 2 + 2;         // 4
print a + b;         // Prints the sum of variables a and b
```

## Implementation Details

### Architecture

NRK is implemented as a bytecode virtual machine with a stack-based architecture:

1. **Scanner**: Tokenizes source code into a stream of tokens
2. **Compiler**: Parses tokens and generates bytecode instructions
3. **Virtual Machine**: Executes bytecode instructions

### Project Structure

- **VM**: Stack-based virtual machine that executes bytecode
- **Compiler**: Turns source code into bytecode
- **Scanner**: Tokenizes source code for the compiler
- **Chunk**: Represents bytecode and related metadata
- **Value**: Represents runtime values (numbers, booleans, nil, strings)
- **Object**: Manages heap-allocated objects like strings
- **Memory**: Handles memory allocation, reallocation, and garbage collection
- **Debug**: Tools for inspecting bytecode and execution
- **REPL**: Interactive environment with history, line editing, and history persistence
- **Table**: Hash table implementation for string interning and variable lookup

### Development Status

NRK is currently in early development (v0.0.1) and implements:

- Basic arithmetic operations
- Comparison operations
- Boolean logic
- Bitwise operations
- Variables and assignment
- Print statements
- Postfix operators
- String operations and template strings
- String interning with hash tables
- Memory management foundations with garbage collection

### Debugging

The interpreter includes debugging features that can be enabled by uncommenting the corresponding define statements in `common.h`:

- `DEBUG_PRINT_CODE`: Disassembles and prints bytecode after compilation
- `DEBUG_TRACE_EXECUTION`: Traces VM execution step by step, showing stack state
- `DEBUG_SCAN_EXECUTION`: Shows detailed scanning process information
- `DEBUG_COMPILE_EXECUTION`: Provides detailed compilation process logs

## Roadmap

Future plans include:

- Control flow (if/else, loops)
- Functions and closures
- Classes and inheritance
- Full implementation of compound assignment operators

## Development Notes

- String interning is implemented using a hash table for efficient string comparison
- Extended constant pool via `OP_CONSTANT_LONG` allows for more than 256 constants
- Memory management uses Flexible Array Members (FAM) for efficient string storage
- Planned improvements:
  - Extend table support to other immutable objects besides strings as keys
  - Optimize compiler pointer indirections in critical paths
  - Complete implementation of compound assignment operators
