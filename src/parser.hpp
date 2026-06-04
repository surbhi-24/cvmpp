#pragma once
#include "token.hpp"
#include "ast.hpp"
#include <vector>
#include <stdexcept>

// Recursive-descent parser.
// Grammar (informal):
//   program   := (func_decl | statement)* END
//   func_decl := 'func' NAME '(' params ')' block
//   block     := '{' statement* '}'
//   statement := let | assign | if | while | print | return | expr_stmt
//   expr      := comparison
//   comparison := addition (('=='|'!='|'<'|'>'|'<='|'>=') addition)*
//   addition   := term (('+' | '-') term)*
//   term       := unary (('*' | '/' | '%') unary)*
//   unary      := '-' unary | primary
//   primary    := NUMBER | BOOL | STRING | NAME | call | read | '(' expr ')'

class Parser {
public:
    explicit Parser(std::vector<Token> tokens)
        : tokens_(std::move(tokens)), pos_(0) {}

    std::vector<StmtPtr> parse() {
        std::vector<StmtPtr> program;
        while (!at(TK::END)) {
            program.push_back(parse_top_level());
        }
        return program;
    }

private:
    std::vector<Token> tokens_;
    size_t             pos_;

    // ── Token navigation ─────────────────────────────────────────────────────

    const Token& cur() const { return tokens_[pos_]; }

    bool at(TK k) const { return cur().kind == k; }

    Token consume() {
        Token t = cur();
        ++pos_;
        return t;
    }

    Token expect(TK k, const std::string& what) {
        if (!at(k))
            throw std::runtime_error(
                "line " + std::to_string(cur().line) +
                ": expected " + what + ", got '" + cur().text + "'");
        return consume();
    }

    bool match(TK k) {
        if (at(k)) { consume(); return true; }
        return false;
    }

    // ── Top-level ─────────────────────────────────────────────────────────────

    StmtPtr parse_top_level() {
        if (at(TK::FUNC)) return parse_func_decl();
        return parse_statement();
    }

    // ── Function declaration ──────────────────────────────────────────────────

    StmtPtr parse_func_decl() {
        int ln = cur().line;
        expect(TK::FUNC, "'func'");
        std::string name = expect(TK::NAME, "function name").text;
        expect(TK::LPAREN, "'('");

        std::vector<std::string> params;
        while (!at(TK::RPAREN) && !at(TK::END)) {
            params.push_back(expect(TK::NAME, "parameter name").text);
            if (!match(TK::COMMA)) break;
        }
        expect(TK::RPAREN, "')'");

        auto body = parse_block();
        return make_stmt(FuncDecl{ name, std::move(params), std::move(body) }, ln);
    }

    // ── Block  ────────────────────────────────────────────────────────────────

    std::vector<StmtPtr> parse_block() {
        expect(TK::LBRACE, "'{'");
        std::vector<StmtPtr> body;
        while (!at(TK::RBRACE) && !at(TK::END))
            body.push_back(parse_statement());
        expect(TK::RBRACE, "'}'");
        return body;
    }

    // ── Statements ────────────────────────────────────────────────────────────

    StmtPtr parse_statement() {
        int ln = cur().line;

        if (at(TK::LET))    return parse_let();
        if (at(TK::IF))     return parse_if();
        if (at(TK::WHILE))  return parse_while();
        if (at(TK::PRINT))  return parse_print();
        if (at(TK::RETURN)) return parse_return();

        // assignment vs bare expression (function call as statement)
        if (at(TK::NAME) && pos_ + 1 < tokens_.size() && tokens_[pos_+1].kind == TK::ASSIGN) {
            std::string name = consume().text;
            consume(); // eat '='
            auto val = parse_expr();
            expect(TK::SEMI, "';'");
            return make_stmt(AssignStmt{ name, std::move(val) }, ln);
        }

        // bare expression statement (e.g. a function call for side effects)
        auto e = parse_expr();
        expect(TK::SEMI, "';'");
        return make_stmt(ExprStmt{ std::move(e) }, ln);
    }

    StmtPtr parse_let() {
        int ln = cur().line;
        expect(TK::LET, "'let'");
        std::string name = expect(TK::NAME, "variable name").text;
        expect(TK::ASSIGN, "'='");
        auto init = parse_expr();
        expect(TK::SEMI, "';'");
        return make_stmt(LetStmt{ name, std::move(init) }, ln);
    }

    StmtPtr parse_if() {
        int ln = cur().line;
        expect(TK::IF, "'if'");
        expect(TK::LPAREN, "'('");
        auto cond = parse_expr();
        expect(TK::RPAREN, "')'");
        auto then_body = parse_block();
        std::vector<StmtPtr> else_body;
        if (match(TK::ELSE)) {
            if (at(TK::IF))
                else_body.push_back(parse_if());  // else if chain
            else
                else_body = parse_block();
        }
        return make_stmt(IfStmt{ std::move(cond), std::move(then_body), std::move(else_body) }, ln);
    }

    StmtPtr parse_while() {
        int ln = cur().line;
        expect(TK::WHILE, "'while'");
        expect(TK::LPAREN, "'('");
        auto cond = parse_expr();
        expect(TK::RPAREN, "')'");
        auto body = parse_block();
        return make_stmt(WhileStmt{ std::move(cond), std::move(body) }, ln);
    }

    StmtPtr parse_print() {
        int ln = cur().line;
        expect(TK::PRINT, "'print'");
        expect(TK::LPAREN, "'('");
        auto val = parse_expr();
        expect(TK::RPAREN, "')'");
        expect(TK::SEMI, "';'");
        return make_stmt(PrintStmt{ std::move(val) }, ln);
    }

    StmtPtr parse_return() {
        int ln = cur().line;
        expect(TK::RETURN, "'return'");
        if (match(TK::SEMI))
            return make_stmt(ReturnStmt{ nullptr }, ln);
        auto val = parse_expr();
        expect(TK::SEMI, "';'");
        return make_stmt(ReturnStmt{ std::move(val) }, ln);
    }

    // ── Expressions (precedence climbing) ─────────────────────────────────────

    ExprPtr parse_expr() { return parse_comparison(); }

    ExprPtr parse_comparison() {
        auto left = parse_addition();
        while (true) {
            std::string op;
            if      (at(TK::EQEQ)) op = "==";
            else if (at(TK::NEQ))  op = "!=";
            else if (at(TK::LT))   op = "<";
            else if (at(TK::GT))   op = ">";
            else if (at(TK::LTE))  op = "<=";
            else if (at(TK::GTE))  op = ">=";
            else break;
            int ln = cur().line; consume();
            auto right = parse_addition();
            left = make_expr(BinOp{ op, std::move(left), std::move(right) }, ln);
        }
        return left;
    }

    ExprPtr parse_addition() {
        auto left = parse_term();
        while (at(TK::PLUS) || at(TK::MINUS)) {
            std::string op = at(TK::PLUS) ? "+" : "-";
            int ln = cur().line; consume();
            auto right = parse_term();
            left = make_expr(BinOp{ op, std::move(left), std::move(right) }, ln);
        }
        return left;
    }

    ExprPtr parse_term() {
        auto left = parse_unary();
        while (at(TK::STAR) || at(TK::SLASH) || at(TK::PERCENT)) {
            std::string op = at(TK::STAR) ? "*" : at(TK::SLASH) ? "/" : "%";
            int ln = cur().line; consume();
            auto right = parse_unary();
            left = make_expr(BinOp{ op, std::move(left), std::move(right) }, ln);
        }
        return left;
    }

    ExprPtr parse_unary() {
        if (at(TK::MINUS)) {
            int ln = cur().line; consume();
            auto operand = parse_unary();
            return make_expr(UnaryMinus{ std::move(operand) }, ln);
        }
        return parse_primary();
    }

    ExprPtr parse_primary() {
        int ln = cur().line;

        if (at(TK::NUMBER)) {
            int64_t v = std::stoll(cur().text);
            consume();
            return make_expr(NumLit{ v }, ln);
        }
        if (at(TK::BOOL_TRUE))  { consume(); return make_expr(BoolLit{ true },  ln); }
        if (at(TK::BOOL_FALSE)) { consume(); return make_expr(BoolLit{ false }, ln); }
        if (at(TK::STRING)) {
            std::string s = cur().text; consume();
            return make_expr(StrLit{ s }, ln);
        }
        if (at(TK::INPUT)) {
            consume();
            expect(TK::LPAREN, "'('");
            expect(TK::RPAREN, "')'");
            return make_expr(InputExpr{}, ln);
        }
        if (at(TK::NAME)) {
            std::string name = consume().text;
            // function call?
            if (match(TK::LPAREN)) {
                std::vector<ExprPtr> args;
                while (!at(TK::RPAREN) && !at(TK::END)) {
                    args.push_back(parse_expr());
                    if (!match(TK::COMMA)) break;
                }
                expect(TK::RPAREN, "')'");
                return make_expr(FuncCall{ name, std::move(args) }, ln);
            }
            return make_expr(VarRef{ name }, ln);
        }
        if (match(TK::LPAREN)) {
            auto e = parse_expr();
            expect(TK::RPAREN, "')'");
            return e;
        }

        throw std::runtime_error(
            "line " + std::to_string(ln) +
            ": unexpected token '" + cur().text + "'");
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    template<typename T>
    ExprPtr make_expr(T&& node, int line) {
        return std::make_unique<Expr>(Expr{ ExprNode{ std::forward<T>(node) }, line });
    }

    template<typename T>
    StmtPtr make_stmt(T&& node, int line) {
        return std::make_unique<Stmt>(Stmt{ StmtNode{ std::forward<T>(node) }, line });
    }
};
