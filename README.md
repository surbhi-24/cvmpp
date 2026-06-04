# CVM++ — Custom Scripting Language & Bytecode VM

A hand-built compiler toolchain written in **C++17** with **zero external dependencies**.

Write `.cvm` scripts → they are tokenized, parsed into an AST, compiled to stack bytecode, and executed on a custom virtual machine.

```
source.cvm  →  Lexer  →  Parser  →  Compiler  →  Stack VM  →  stdout
               tokens     AST       bytecode
```

---

## Tech Stack

| Layer | Details |
|---|---|
| **Language** | C++17 — no external libraries |
| **Build System** | `g++` / `Makefile` / CMake (optional) |
| **Core Concepts** | Lexical analysis, recursive-descent parsing, `std::variant`-based AST, single-pass bytecode generation, backpatching, positional stack frames |

---

## Language Features

| Feature | Syntax |
|---|---|
| **Variables** | `let x = 10;` |
| **Integers** | `42`, `-7` |
| **Booleans** | `true`, `false` |
| **Strings** | `"hello"` |
| **Arithmetic** | `+ - * / %` |
| **Comparison** | `== != < > <= >=` |
| **If / Else** | `if (cond) { } else { }` |
| **While Loop** | `while (cond) { }` |
| **Functions** | `func name(a, b) { return a + b; }` |
| **Print** | `print(expr);` |
| **Input** | `let x = input();` |
| **Comments** | `// single line` |

---

## Build

```bash
# Using Makefile
make

# Manually
g++ -std=c++17 -Wall -Wextra -O2 -I src -o build/cvmpp src/main.cpp
```

---

## Usage

```bash
# Run a script
./build/cvmpp examples/hello.cvm

# Show compiled bytecode before running
./build/cvmpp --debug examples/factorial.cvm

# Print each VM instruction as it executes
./build/cvmpp --trace examples/fizzbuzz.cvm

# Start the interactive REPL
./build/cvmpp
```

**REPL commands:** `debug` (toggle disassembler), `trace` (toggle execution log), `clear` (reset session), `exit`

---

## Examples

```bash
make run-hello        # Hello, World!
make run-factorial    # 10! = 3628800
make run-fizzbuzz     # FizzBuzz up to 20
make run-functions    # recursive factorial and fibonacci
make run-assignment   # variable reassignment
make run-comparisons  # comparison operators
make run-div-by-zero  # runtime error handling
make run-multiline    # multi-line while loop
make verify           # run all 12 examples
```

---

## How It Works

### Lexer `src/lexer.hpp`
Scans source text character by character and produces a flat list of typed tokens. Handles keywords, identifiers, number literals, strings, operators, and `//` comments.

### Parser `src/parser.hpp`
Recursive-descent parser where each grammar rule maps to its own function. Operator precedence is encoded by the call hierarchy:

```
comparison → addition/subtraction → multiplication/division/modulo → unary → primary
```

Builds an AST made entirely of `std::variant` nodes — no virtual inheritance.

### AST `src/ast.hpp`
All nodes use `std::variant`, which means unhandled cases are caught at compile time rather than silently failing at runtime.

| Layer | Node Types |
|---|---|
| **Expressions** | `NumLit`, `BoolLit`, `StrLit`, `VarRef`, `BinOp`, `UnaryMinus`, `FuncCall`, `InputExpr` |
| **Statements** | `LetStmt`, `AssignStmt`, `IfStmt`, `WhileStmt`, `FuncDecl`, `PrintStmt`, `ReturnStmt` |

### Compiler `src/compiler.hpp`
Walks the AST and emits bytecode into a flat `Chunk`. Key techniques:

- **Backpatching** — `if` and `while` need forward jumps whose target addresses aren't known yet. A placeholder `0` is emitted, the body is compiled, then the real address is patched in.
- **Name interning** — global variable names are stored in a pool and referenced by 2-byte index.
- **Positional locals** — function parameters and local variables get stack slot numbers at compile time, so the VM never does name lookups inside functions.

### VM `src/vm.hpp`
Runs bytecode through a `switch`-based fetch-decode-execute loop. Values are `std::variant<int64_t, bool, std::string>`. Function calls push a `Frame` (return address + stack base) onto a call-frame stack; `RETURN` pops it and restores the previous context.

### Chunk `src/chunk.hpp`
Holds the raw bytecode alongside three constant pools: numbers, strings, and interned global names. Provides `patch_u32` for backpatching jump targets.

---

## Architecture Reference

```
Chunk {
  code[]        — raw bytecode bytes
  numbers[]     — int64 constant pool
  strings[]     — string constant pool
  names[]       — interned global variable names
}

FuncProto {
  name, param_count, local_count, entry (byte offset)
}

VM state {
  stack[]       — Value (int64 | bool | string)
  frames[]      — Frame (return_pc, stack_base, local_count)
  globals{}     — map<string, Value>
}
```

---

## Opcodes (26 total)

| Category | Opcodes |
|---|---|
| Push | `PUSH_NUM` `PUSH_BOOL` `PUSH_STR` |
| Variables | `LOAD_GLOBAL` `STORE_GLOBAL` `LOAD_LOCAL` `STORE_LOCAL` |
| Arithmetic | `ADD` `SUB` `MUL` `DIV` `MOD` |
| Comparison | `EQ` `NE` `LT` `GT` `LE` `GE` |
| Control flow | `JUMP` `JUMP_FALSE` |
| Functions | `CALL` `RETURN` |
| I/O | `PRINT` `INPUT` |
| Stack | `POP` `HALT` |

---

## Exit Codes

| Code | Meaning |
|:---:|---|
| `0` | Success |
| `1` | Compile error or runtime error (e.g. division by zero) |
