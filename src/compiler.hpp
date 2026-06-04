#pragma once
#include "ast.hpp"
#include "chunk.hpp"
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <string>

// Compiles a list of top-level statements into a Chunk + function table.
// Design:
//   - Global variables  → name-indexed slot in VM's globals table
//   - Function locals   → positional slot in call frame
//   - Functions compiled first, main code at the end
//   - No separate "entry JUMP over functions" — functions are stored
//     separately by entry address; the VM starts execution at main_entry

class Compiler {
public:
    struct Result {
        Chunk                  chunk;
        std::vector<FuncProto> funcs;
        size_t                 main_entry;  // where to start executing
    };

    Result compile(const std::vector<StmtPtr>& program) {
        // First pass: register all func declarations so forward calls work
        for (const auto& s : program) {
            if (auto* fd = std::get_if<FuncDecl>(&s->node))
                register_func(fd->name, (int)fd->params.size());
        }

        // Compile functions first (they sit before main in the bytecode)
        for (const auto& s : program) {
            if (auto* fd = std::get_if<FuncDecl>(&s->node))
                compile_func(*fd);
        }

        // Main starts here
        size_t main_start = chunk_.here();

        // Compile everything that isn't a func declaration
        for (const auto& s : program) {
            if (!std::get_if<FuncDecl>(&s->node))
                compile_stmt(*s);
        }

        chunk_.emit_op(Op::HALT);

        return Result{ std::move(chunk_), std::move(funcs_), main_start };
    }

private:
    Chunk                  chunk_;
    std::vector<FuncProto> funcs_;

    // For compiling a function body: maps local name → slot index
    std::vector<std::unordered_map<std::string,int>> local_scopes_;
    int current_local_count_ = 0;

    bool in_function() const { return !local_scopes_.empty(); }

    // ── Function registration ────────────────────────────────────────────────

    void register_func(const std::string& name, int param_count) {
        funcs_.push_back({ name, param_count, 0, 0 });
    }

    int func_index(const std::string& name) const {
        for (int i = 0; i < (int)funcs_.size(); ++i)
            if (funcs_[i].name == name) return i;
        return -1;
    }

    // ── Compile function declaration ─────────────────────────────────────────

    void compile_func(const FuncDecl& fd) {
        int idx = func_index(fd.name);
        funcs_[idx].entry = chunk_.here();

        // Set up local scope: params first
        local_scopes_.emplace_back();
        current_local_count_ = 0;

        for (const auto& p : fd.params) {
            local_scopes_.back()[p] = current_local_count_++;
        }

        for (const auto& s : fd.body)
            compile_stmt(*s);

        // Implicit return
        chunk_.emit_op(Op::PUSH_NUM);
        chunk_.emit_i64(0);
        chunk_.emit_op(Op::RETURN);

        funcs_[idx].local_count = current_local_count_;
        local_scopes_.pop_back();
        current_local_count_ = 0;
    }

    // ── Statements ───────────────────────────────────────────────────────────

    void compile_stmt(const Stmt& s) {
        std::visit([&](auto& node) { compile_node(node); }, s.node);
    }

    void compile_node(const LetStmt& n) {
        compile_expr(*n.init);
        if (in_function()) {
            // new local slot
            local_scopes_.back()[n.name] = current_local_count_++;
            chunk_.emit_op(Op::STORE_LOCAL);
            chunk_.emit_byte(static_cast<uint8_t>(current_local_count_ - 1));
        } else {
            chunk_.emit_op(Op::STORE_GLOBAL);
            chunk_.emit_u16(chunk_.intern_name(n.name));
        }
    }

    void compile_node(const AssignStmt& n) {
        compile_expr(*n.value);
        store_var(n.name);
    }

    void compile_node(const PrintStmt& n) {
        compile_expr(*n.value);
        chunk_.emit_op(Op::PRINT);
    }

    void compile_node(const ReturnStmt& n) {
        if (n.value) compile_expr(*n.value);
        else {
            chunk_.emit_op(Op::PUSH_NUM);
            chunk_.emit_i64(0);
        }
        chunk_.emit_op(Op::RETURN);
    }

    void compile_node(const ExprStmt& n) {
        compile_expr(*n.expr);
        chunk_.emit_op(Op::POP);  // discard result
    }

    void compile_node(const IfStmt& n) {
        compile_expr(*n.cond);

        // JUMP_FALSE over then-body
        chunk_.emit_op(Op::JUMP_FALSE);
        size_t jump_false_addr = chunk_.here();
        chunk_.emit_u32(0); // placeholder

        for (const auto& s : n.then_body) compile_stmt(*s);

        if (!n.else_body.empty()) {
            // Jump over else from end of then
            chunk_.emit_op(Op::JUMP);
            size_t jump_end_addr = chunk_.here();
            chunk_.emit_u32(0);

            chunk_.patch_u32(jump_false_addr, (uint32_t)chunk_.here());
            for (const auto& s : n.else_body) compile_stmt(*s);
            chunk_.patch_u32(jump_end_addr, (uint32_t)chunk_.here());
        } else {
            chunk_.patch_u32(jump_false_addr, (uint32_t)chunk_.here());
        }
    }

    void compile_node(const WhileStmt& n) {
        size_t loop_top = chunk_.here();

        compile_expr(*n.cond);

        // Jump out if false
        chunk_.emit_op(Op::JUMP_FALSE);
        size_t exit_addr = chunk_.here();
        chunk_.emit_u32(0);

        for (const auto& s : n.body) compile_stmt(*s);

        // Loop back
        chunk_.emit_op(Op::JUMP);
        chunk_.emit_u32((uint32_t)loop_top);

        // Patch exit
        chunk_.patch_u32(exit_addr, (uint32_t)chunk_.here());
    }

    // FuncDecl handled at top-level, not inline
    void compile_node(const FuncDecl&) {}

    // ── Expressions ──────────────────────────────────────────────────────────

    void compile_expr(const Expr& e) {
        std::visit([&](auto& node) { compile_expr_node(node, e.line); }, e.node);
    }

    void compile_expr_node(const NumLit& n, int) {
        chunk_.emit_op(Op::PUSH_NUM);
        chunk_.emit_i64(n.value);
    }

    void compile_expr_node(const BoolLit& n, int) {
        chunk_.emit_op(Op::PUSH_BOOL);
        chunk_.emit_byte(n.value ? 1 : 0);
    }

    void compile_expr_node(const StrLit& n, int) {
        chunk_.emit_op(Op::PUSH_STR);
        chunk_.emit_u16(chunk_.intern_string(n.value));
    }

    void compile_expr_node(const VarRef& n, int line) {
        load_var(n.name, line);
    }

    void compile_expr_node(const InputExpr&, int) {
        chunk_.emit_op(Op::INPUT);
    }

    void compile_expr_node(const BinOp& n, int) {
        compile_expr(*n.left);
        compile_expr(*n.right);
        if      (n.op == "+")  chunk_.emit_op(Op::ADD);
        else if (n.op == "-")  chunk_.emit_op(Op::SUB);
        else if (n.op == "*")  chunk_.emit_op(Op::MUL);
        else if (n.op == "/")  chunk_.emit_op(Op::DIV);
        else if (n.op == "%")  chunk_.emit_op(Op::MOD);
        else if (n.op == "==") chunk_.emit_op(Op::EQ);
        else if (n.op == "!=") chunk_.emit_op(Op::NE);
        else if (n.op == "<")  chunk_.emit_op(Op::LT);
        else if (n.op == ">")  chunk_.emit_op(Op::GT);
        else if (n.op == "<=") chunk_.emit_op(Op::LE);
        else if (n.op == ">=") chunk_.emit_op(Op::GE);
    }

    void compile_expr_node(const UnaryMinus& n, int) {
        // Push 0 and subtract: 0 - x
        chunk_.emit_op(Op::PUSH_NUM);
        chunk_.emit_i64(0);
        compile_expr(*n.operand);
        chunk_.emit_op(Op::SUB);
    }

    void compile_expr_node(const FuncCall& n, int line) {
        int idx = func_index(n.name);
        if (idx < 0)
            throw std::runtime_error(
                "line " + std::to_string(line) + ": unknown function '" + n.name + "'");
        if ((int)n.args.size() != funcs_[idx].param_count)
            throw std::runtime_error(
                "line " + std::to_string(line) + ": '" + n.name + "' expects " +
                std::to_string(funcs_[idx].param_count) + " arg(s), got " +
                std::to_string(n.args.size()));
        for (const auto& a : n.args) compile_expr(*a);
        chunk_.emit_op(Op::CALL);
        chunk_.emit_u16((uint16_t)idx);
    }

    // ── Variable load/store helpers ───────────────────────────────────────────

    int local_slot(const std::string& name) const {
        if (local_scopes_.empty()) return -1;
        auto it = local_scopes_.back().find(name);
        if (it == local_scopes_.back().end()) return -1;
        return it->second;
    }

    void load_var(const std::string& name, int line) {
        (void)line; // Explicitly suppress the unused-parameter warning
        int slot = local_slot(name);
        if (slot >= 0) {
            chunk_.emit_op(Op::LOAD_LOCAL);
            chunk_.emit_byte((uint8_t)slot);
        } else {
            // check global exists? We can't at compile time easily, let VM handle it
            chunk_.emit_op(Op::LOAD_GLOBAL);
            chunk_.emit_u16(chunk_.intern_name(name));
        }
    }

    void store_var(const std::string& name) {
        int slot = local_slot(name);
        if (slot >= 0) {
            chunk_.emit_op(Op::STORE_LOCAL);
            chunk_.emit_byte((uint8_t)slot);
        } else {
            chunk_.emit_op(Op::STORE_GLOBAL);
            chunk_.emit_u16(chunk_.intern_name(name));
        }
    }
};
