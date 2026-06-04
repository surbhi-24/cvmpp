#pragma once
#include "token.hpp"
#include <vector>
#include <stdexcept>
#include <cctype>

// Turns raw source text into a flat list of Tokens.
// Throws std::runtime_error on bad input with a line number.

class Lexer {
public:
    explicit Lexer(std::string src)
        : src_(std::move(src)), cur_(0), line_(1) {}

    std::vector<Token> scan() {
        std::vector<Token> out;
        while (cur_ < src_.size()) {
            skip_space_and_comments();
            if (cur_ >= src_.size()) break;

            char c = src_[cur_];

            if (std::isdigit(c))           { out.push_back(read_number());  continue; }
            if (std::isalpha(c) || c=='_') { out.push_back(read_word());    continue; }
            if (c == '"')                  { out.push_back(read_string());  continue; }

            // Single/double-char punctuation
            switch (c) {
                case '+': emit(out, TK::PLUS,   "+"); break;
                case '-': emit(out, TK::MINUS,  "-"); break;
                case '*': emit(out, TK::STAR,   "*"); break;
                case '/': emit(out, TK::SLASH,  "/"); break;
                case '%': emit(out, TK::PERCENT,"%"); break;
                case '(': emit(out, TK::LPAREN, "("); break;
                case ')': emit(out, TK::RPAREN, ")"); break;
                case '{': emit(out, TK::LBRACE, "{"); break;
                case '}': emit(out, TK::RBRACE, "}"); break;
                case ',': emit(out, TK::COMMA,  ","); break;
                case ';': emit(out, TK::SEMI,   ";"); break;

                case '=':
                    if (peek() == '=') { advance(); emit(out, TK::EQEQ,  "=="); }
                    else                              emit(out, TK::ASSIGN,"=");
                    break;
                case '!':
                    expect('='); emit(out, TK::NEQ, "!="); break;
                case '<':
                    if (peek() == '=') { advance(); emit(out, TK::LTE, "<="); }
                    else                             emit(out, TK::LT,  "<");
                    break;
                case '>':
                    if (peek() == '=') { advance(); emit(out, TK::GTE, ">="); }
                    else                             emit(out, TK::GT,  ">");
                    break;

                default:
                    throw std::runtime_error(
                        "line " + std::to_string(line_) +
                        ": unexpected character '" + c + "'");
            }
            advance();
        }
        out.emplace_back(TK::END, "", line_);
        return out;
    }

private:
    std::string src_;
    size_t      cur_;
    int         line_;

    char peek(int offset = 1) const {
        size_t idx = cur_ + offset;
        return idx < src_.size() ? src_[idx] : '\0';
    }

    void advance() {
        if (cur_ < src_.size() && src_[cur_] == '\n') ++line_;
        ++cur_;
    }

    void skip_space_and_comments() {
        while (cur_ < src_.size()) {
            if (std::isspace(src_[cur_])) { advance(); continue; }
            // single-line comment
            if (src_[cur_] == '/' && peek() == '/') {
                while (cur_ < src_.size() && src_[cur_] != '\n') advance();
                continue;
            }
            break;
        }
    }

    // Consume current char and add token
    void emit(std::vector<Token>& out, TK k, std::string text) {
        out.emplace_back(k, std::move(text), line_);
    }

    void expect(char c) {
        advance();
        if (cur_ >= src_.size() || src_[cur_] != c)
            throw std::runtime_error(
                "line " + std::to_string(line_) +
                ": expected '" + c + "'");
    }

    Token read_number() {
        size_t start = cur_;
        while (cur_ < src_.size() && std::isdigit(src_[cur_])) advance();
        return Token(TK::NUMBER, src_.substr(start, cur_ - start), line_);
    }

    Token read_word() {
        size_t start = cur_;
        while (cur_ < src_.size() && (std::isalnum(src_[cur_]) || src_[cur_] == '_'))
            advance();
        std::string w = src_.substr(start, cur_ - start);

        // keyword table
        if (w == "let")    return Token(TK::LET,       w, line_);
        if (w == "if")     return Token(TK::IF,        w, line_);
        if (w == "else")   return Token(TK::ELSE,      w, line_);
        if (w == "while")  return Token(TK::WHILE,     w, line_);
        if (w == "func")   return Token(TK::FUNC,      w, line_);
        if (w == "return") return Token(TK::RETURN,    w, line_);
        if (w == "print")  return Token(TK::PRINT,     w, line_);
        if (w == "input")   return Token(TK::INPUT,      w, line_);
        if (w == "true")   return Token(TK::BOOL_TRUE, w, line_);
        if (w == "false")  return Token(TK::BOOL_FALSE,w, line_);

        return Token(TK::NAME, w, line_);
    }

    Token read_string() {
        advance(); // skip opening "
        size_t start = cur_;
        while (cur_ < src_.size() && src_[cur_] != '"') {
            if (src_[cur_] == '\n')
                throw std::runtime_error("line " + std::to_string(line_) + ": unterminated string");
            advance();
        }
        std::string content = src_.substr(start, cur_ - start);
        if (cur_ < src_.size()) advance(); // skip closing "
        return Token(TK::STRING, content, line_);
    }
};
