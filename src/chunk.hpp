#pragma once
#include <cstdint>
#include <vector>
#include <string>

// Every operation the VM can perform
enum class Op : uint8_t {
    // Push literals onto stack
    PUSH_NUM,       // operand: int64 (8 bytes)
    PUSH_BOOL,      // operand: 0/1 (1 byte)
    PUSH_STR,       // operand: string index (2 bytes)

    // Variables  (globals use name index, locals use slot index)
    LOAD_GLOBAL,    // operand: name index (2 bytes)
    STORE_GLOBAL,   // operand: name index (2 bytes)
    LOAD_LOCAL,     // operand: slot index (1 byte)
    STORE_LOCAL,    // operand: slot index (1 byte)

    // Arithmetic
    ADD, SUB, MUL, DIV, MOD,

    // Comparison → pushes bool
    EQ, NE, LT, GT, LE, GE,

    // Control flow  (operand: absolute address, 4 bytes)
    JUMP,           // unconditional
    JUMP_FALSE,     // pop + jump if false

    // Functions
    CALL,           // operand: func index (2 bytes)
    RETURN,

    // I/O
    PRINT,
    INPUT,

    // Stack
    POP,
    HALT,
};

// A "Chunk" holds compiled bytecode + constant pools
struct Chunk {
    std::vector<uint8_t>  code;       // raw bytecode bytes
    std::vector<int64_t>  numbers;    // number constant pool
    std::vector<std::string> strings; // string constant pool
    std::vector<std::string> names;   // global variable name pool

    // Emit helpers
    void emit_byte(uint8_t b)           { code.push_back(b); }
    void emit_op(Op op)                 { code.push_back(static_cast<uint8_t>(op)); }

    void emit_i64(int64_t v) {
        for (int i = 0; i < 8; ++i)
            code.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }

    void emit_u32(uint32_t v) {
        for (int i = 0; i < 4; ++i)
            code.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }

    void emit_u16(uint16_t v) {
        code.push_back(static_cast<uint8_t>(v & 0xFF));
        code.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    // Patch a previously emitted u32 address
    void patch_u32(size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i)
            code[at + i] = static_cast<uint8_t>((v >> (i * 8)) & 0xFF);
    }

    size_t here() const { return code.size(); }

    // Intern a global name; returns its index
    uint16_t intern_name(const std::string& n) {
        for (size_t i = 0; i < names.size(); ++i)
            if (names[i] == n) return static_cast<uint16_t>(i);
        names.push_back(n);
        return static_cast<uint16_t>(names.size() - 1);
    }

    uint16_t intern_string(const std::string& s) {
        for (size_t i = 0; i < strings.size(); ++i)
            if (strings[i] == s) return static_cast<uint16_t>(i);
        strings.push_back(s);
        return static_cast<uint16_t>(strings.size() - 1);
    }

    // Read back helpers (used by VM)
    int64_t  read_i64(size_t pc) const {
        int64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= (int64_t)code[pc + i] << (i * 8);
        return v;
    }
    uint32_t read_u32(size_t pc) const {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= (uint32_t)code[pc + i] << (i * 8);
        return v;
    }
    uint16_t read_u16(size_t pc) const {
        return (uint16_t)code[pc] | ((uint16_t)code[pc+1] << 8);
    }
};

// Metadata for a compiled function
struct FuncProto {
    std::string name;
    int         param_count;
    int         local_count;
    size_t      entry;   // byte offset into Chunk::code where this func starts
};
