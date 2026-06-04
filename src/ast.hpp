#pragma once
#include <memory>
#include <vector>
#include <string>
#include <variant>

// Forward declarations
struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// ─── Expression nodes ───────────────────────────────────────────────────────

struct NumLit    { int64_t value; };
struct BoolLit   { bool value; };
struct StrLit    { std::string value; };
struct VarRef    { std::string name; };
struct InputExpr  {};  // input keyword (takes stdin)

struct BinOp {
    std::string op;
    ExprPtr     left, right;
};

struct UnaryMinus {
    ExprPtr operand;
};

struct FuncCall {
    std::string            name;
    std::vector<ExprPtr>   args;
};

using ExprNode = std::variant<
    NumLit, BoolLit, StrLit, VarRef, InputExpr,
    BinOp, UnaryMinus, FuncCall
>;

struct Expr {
    ExprNode node;
    int      line;
};

// ─── Statement nodes ────────────────────────────────────────────────────────

struct LetStmt    { std::string name; ExprPtr init; };
struct AssignStmt { std::string name; ExprPtr value; };
struct PrintStmt  { ExprPtr value; };
struct ReturnStmt { ExprPtr value; };   // value may be nullptr

struct IfStmt {
    ExprPtr              cond;
    std::vector<StmtPtr> then_body;
    std::vector<StmtPtr> else_body;   // empty = no else
};

struct WhileStmt {
    ExprPtr              cond;
    std::vector<StmtPtr> body;
};

struct FuncDecl {
    std::string              name;
    std::vector<std::string> params;
    std::vector<StmtPtr>     body;
};

struct ExprStmt { ExprPtr expr; };  // bare function call as statement

using StmtNode = std::variant<
    LetStmt, AssignStmt, PrintStmt, ReturnStmt,
    IfStmt, WhileStmt, FuncDecl, ExprStmt
>;

struct Stmt {
    StmtNode node;
    int      line;
};
