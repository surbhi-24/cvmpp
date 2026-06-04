#pragma once
#include "chunk.hpp"
#include <vector>
#include <variant>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <string>

// A runtime value is a number, bool, or string
using Value = std::variant<int64_t, bool, std::string>;

// ── Value helpers ─────────────────────────────────────────────────────────────

static std::string value_to_str(const Value& v) {
    if (auto* n = std::get_if<int64_t>(&v)) return std::to_string(*n);
    if (auto* b = std::get_if<bool>(&v))    return *b ? "true" : "false";
    if (auto* s = std::get_if<std::string>(&v)) return *s;
    return "?";
}

static bool value_truthy(const Value& v) {
    if (auto* b = std::get_if<bool>(&v))    return *b;
    if (auto* n = std::get_if<int64_t>(&v)) return *n != 0;
    return true;
}

// ── Call frame ────────────────────────────────────────────────────────────────

struct Frame {
    size_t            return_pc;     // where to resume after RETURN
    size_t            stack_base;    // where this frame's locals start on the value stack
    int               local_count;
};

// ── VM ────────────────────────────────────────────────────────────────────────

class VM {
public:
    static constexpr size_t MAX_STACK = 65536;
    static constexpr size_t MAX_STEPS = 10'000'000;

    void run(const Chunk& chunk,
             const std::vector<FuncProto>& funcs,
             size_t entry,
             bool trace = false)
    {
        chunk_  = &chunk;
        funcs_  = &funcs;
        pc_     = entry;
        steps_  = 0;

        while (true) {
            if (++steps_ > MAX_STEPS)
                throw std::runtime_error("step limit exceeded — possible infinite loop");

            auto op = static_cast<Op>(read_byte());

            if (trace) print_trace(op);

            switch (op) {

            // ── Push literals ──────────────────────────────────────────────
            case Op::PUSH_NUM: {
                int64_t v = chunk.read_i64(pc_); pc_ += 8;
                push(v);
                break;
            }
            case Op::PUSH_BOOL: {
                bool b = read_byte() != 0;
                push(b);
                break;
            }
            case Op::PUSH_STR: {
                uint16_t idx = chunk.read_u16(pc_); pc_ += 2;
                push(chunk.strings.at(idx));
                break;
            }

            // ── Global variables ──────────────────────────────────────────
            case Op::LOAD_GLOBAL: {
                uint16_t idx = chunk.read_u16(pc_); pc_ += 2;
                const std::string& name = chunk.names.at(idx);
                auto it = globals_.find(name);
                if (it == globals_.end())
                    throw std::runtime_error("undefined variable '" + name + "'");
                push(it->second);
                break;
            }
            case Op::STORE_GLOBAL: {
                uint16_t idx = chunk.read_u16(pc_); pc_ += 2;
                globals_[chunk.names.at(idx)] = pop();
                break;
            }

            // ── Local variables ───────────────────────────────────────────
            case Op::LOAD_LOCAL: {
                uint8_t slot = read_byte();
                push(stack_[frames_.back().stack_base + slot]);
                break;
            }
            case Op::STORE_LOCAL: {
                uint8_t slot = read_byte();
                stack_[frames_.back().stack_base + slot] = pop();
                // store doesn't pop
                break;
            }

            // ── Arithmetic ────────────────────────────────────────────────
            case Op::ADD: { auto b = pop(); auto a = pop(); push(arith(a, b, "+")); break; }
            case Op::SUB: { auto b = pop(); auto a = pop(); push(arith(a, b, "-")); break; }
            case Op::MUL: { auto b = pop(); auto a = pop(); push(arith(a, b, "*")); break; }
            case Op::DIV: { auto b = pop(); auto a = pop(); push(arith(a, b, "/")); break; }
            case Op::MOD: { auto b = pop(); auto a = pop(); push(arith(a, b, "%")); break; }

            // ── Comparison ────────────────────────────────────────────────
            case Op::EQ: { auto b = pop(); auto a = pop(); push(a == b);              break; }
            case Op::NE: { auto b = pop(); auto a = pop(); push(a != b);              break; }
            case Op::LT: { auto b = pop(); auto a = pop(); push(num(a) <  num(b));    break; }
            case Op::GT: { auto b = pop(); auto a = pop(); push(num(a) >  num(b));    break; }
            case Op::LE: { auto b = pop(); auto a = pop(); push(num(a) <= num(b));    break; }
            case Op::GE: { auto b = pop(); auto a = pop(); push(num(a) >= num(b));    break; }

            // ── Control flow ──────────────────────────────────────────────
            case Op::JUMP: {
                uint32_t addr = chunk.read_u32(pc_);
                pc_ = addr;
                break;
            }
            case Op::JUMP_FALSE: {
                uint32_t addr = chunk.read_u32(pc_); pc_ += 4;
                if (!value_truthy(pop())) pc_ = addr;
                break;
            }

            // ── Function call ─────────────────────────────────────────────
            case Op::CALL: {
                uint16_t fidx = chunk.read_u16(pc_); pc_ += 2;
                const FuncProto& f = funcs.at(fidx);

                // Allocate locals on the stack (args already there)
                size_t base = stack_.size() - f.param_count;
                // Extend stack for remaining locals
                int extra = f.local_count - f.param_count;
                for (int i = 0; i < extra; ++i) push(int64_t(0));

                frames_.push_back({ pc_, base, f.local_count });
                pc_ = f.entry;
                break;
            }
            case Op::RETURN: {
                Value retval = pop();
                if (frames_.empty())
                    throw std::runtime_error("RETURN outside function");
                Frame& f = frames_.back();
                // unwind stack to base
                stack_.resize(f.stack_base);
                pc_ = f.return_pc;
                frames_.pop_back();
                push(retval);
                break;
            }

            // ── I/O ───────────────────────────────────────────────────────
            case Op::PRINT: {
                std::cout << value_to_str(pop()) << "\n";
                break;
            }
            case Op::INPUT: {
                std::string line;
                std::getline(std::cin, line);
                try { push(int64_t(std::stoll(line))); }
                catch (...) { push(line); }
                break;
            }

            // ── Stack ─────────────────────────────────────────────────────
            case Op::POP:  pop();  break;
            case Op::HALT: return;

            default:
                throw std::runtime_error(
                    "unknown opcode 0x" + std::to_string(static_cast<int>(op)));
            }
        }
    }

private:
    const Chunk*              chunk_ = nullptr;
    const std::vector<FuncProto>* funcs_ = nullptr;
    size_t                    pc_    = 0;
    size_t                    steps_ = 0;

    std::vector<Value>                  stack_;
    std::vector<Frame>                  frames_;
    std::unordered_map<std::string, Value> globals_;

    // ── Stack ops ─────────────────────────────────────────────────────────────

    void push(Value v) {
        if (stack_.size() >= MAX_STACK)
            throw std::runtime_error("stack overflow");
        stack_.push_back(std::move(v));
    }

    Value pop() {
        if (stack_.empty()) throw std::runtime_error("stack underflow");
        Value v = std::move(stack_.back());
        stack_.pop_back();
        return v;
    }

    const Value& top() const {
        if (stack_.empty()) throw std::runtime_error("stack underflow");
        return stack_.back();
    }

    // ── Bytecode read helpers ─────────────────────────────────────────────────

    uint8_t read_byte() { return chunk_->code[pc_++]; }

    // ── Arithmetic helper ─────────────────────────────────────────────────────

    static int64_t num(const Value& v) {
        if (auto* n = std::get_if<int64_t>(&v)) return *n;
        throw std::runtime_error("expected a number");
    }

    static Value arith(const Value& a, const Value& b, const std::string& op) {
        // String concatenation with +
        if (op == "+" && std::holds_alternative<std::string>(a))
            return std::get<std::string>(a) + value_to_str(b);

        int64_t na = num(a), nb = num(b);
        if (op == "+") return na + nb;
        if (op == "-") return na - nb;
        if (op == "*") return na * nb;
        if (op == "/") {
            if (nb == 0) throw std::runtime_error("division by zero");
            return na / nb;
        }
        if (op == "%") {
            if (nb == 0) throw std::runtime_error("modulo by zero");
            return na % nb;
        }
        throw std::runtime_error("unknown operator '" + op + "'");
    }

    // ── Trace helper ─────────────────────────────────────────────────────────

    static const char* op_name(Op op) {
        switch (op) {
            case Op::PUSH_NUM:    return "PUSH_NUM";
            case Op::PUSH_BOOL:   return "PUSH_BOOL";
            case Op::PUSH_STR:    return "PUSH_STR";
            case Op::LOAD_GLOBAL: return "LOAD_GLOBAL";
            case Op::STORE_GLOBAL:return "STORE_GLOBAL";
            case Op::LOAD_LOCAL:  return "LOAD_LOCAL";
            case Op::STORE_LOCAL: return "STORE_LOCAL";
            case Op::ADD:         return "ADD";
            case Op::SUB:         return "SUB";
            case Op::MUL:         return "MUL";
            case Op::DIV:         return "DIV";
            case Op::MOD:         return "MOD";
            case Op::EQ:          return "EQ";
            case Op::NE:          return "NE";
            case Op::LT:          return "LT";
            case Op::GT:          return "GT";
            case Op::LE:          return "LE";
            case Op::GE:          return "GE";
            case Op::JUMP:        return "JUMP";
            case Op::JUMP_FALSE:  return "JUMP_FALSE";
            case Op::CALL:        return "CALL";
            case Op::RETURN:      return "RETURN";
            case Op::PRINT:       return "PRINT";
            case Op::INPUT:        return "READ";
            case Op::POP:         return "POP";
            case Op::HALT:        return "HALT";
            default:              return "???";
        }
    }

    void print_trace(Op op) {
        std::cerr << "[trace] pc=" << (pc_-1)
                  << " op=" << op_name(op)
                  << " stack_depth=" << stack_.size() << "\n";
    }
};
