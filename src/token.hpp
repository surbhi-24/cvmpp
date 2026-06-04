#pragma once
#include <string>

// Every meaningful unit the lexer can produce
enum class TK {
    // Literals
    NUMBER, BOOL_TRUE, BOOL_FALSE, STRING,

    // Identifiers / Keywords
    NAME,
    LET, IF, ELSE, WHILE, FUNC, RETURN,
    PRINT, INPUT,

    // Arithmetic
    PLUS, MINUS, STAR, SLASH, PERCENT,

    // Comparison
    EQEQ, NEQ, LT, GT, LTE, GTE,

    // Assignment & delimiters
    ASSIGN,
    LPAREN, RPAREN, LBRACE, RBRACE,
    COMMA, SEMI,

    END   // end of source
};

struct Token {
    TK          kind;
    std::string text;   // raw text (identifier name, number digits, string content)
    int         line;

    Token(TK k, std::string t, int ln)
        : kind(k), text(std::move(t)), line(ln) {}
};
