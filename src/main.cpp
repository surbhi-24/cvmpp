#include "lexer.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "vm.hpp"
#include "disasm.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// ── Run source string ─────────────────────────────────────────────────────────

static bool run_source(const std::string& src, bool dump, bool trace) {
    try {
        Lexer lex(src);
        auto tokens = lex.scan();

        Parser parser(std::move(tokens));
        auto ast = parser.parse();

        Compiler compiler;
        auto [chunk, funcs, main_entry] = compiler.compile(ast);

        if (dump) disassemble(chunk, funcs, main_entry);

        VM vm;
        vm.run(chunk, funcs, main_entry, trace);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "\033[31mError:\033[0m " << e.what() << "\n";
        return false;
    }
}

// ── Run a .cvm file ───────────────────────────────────────────────────────────

static int run_file(const std::string& path, bool dump, bool trace) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Error: cannot open '" << path << "'\n";
        return 1;
    }
    std::ostringstream buf;
    buf << f.rdbuf();
    return run_source(buf.str(), dump, trace) ? 0 : 1;
}

// ── REPL ─────────────────────────────────────────────────────────────────────
// Strategy: accumulate lines; try to run on every complete-looking input
// (ends with ';' or '}').  On success keep the source in session so variables
// persist between inputs (we recompile from scratch each time).

static void run_repl() {
    std::cout << "cvm++  —  type 'exit' to quit, 'dump' to toggle disasm\n\n";

    bool        dump    = false;
    bool        trace   = false;
    std::string session;   // successfully executed source so far
    std::string pending;   // current unfinished input

    while (true) {
        std::cout << (pending.empty() ? ">> " : ".. ");
        std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line)) break;

        if (line == "exit" || line == "quit") break;

        if (line == "dump")  { dump  = !dump;  std::cout << "  dump "  << (dump  ? "on" : "off") << "\n"; continue; }
        if (line == "trace") { trace = !trace; std::cout << "  trace " << (trace ? "on" : "off") << "\n"; continue; }
        if (line == "clear") { session.clear(); pending.clear(); std::cout << "  session cleared\n"; continue; }

        pending += line + "\n";

        // Heuristic: try running when the line ends with ';' or '}'
        char last = '\0';
        for (char c : line) if (!std::isspace((unsigned char)c)) last = c;
        if (last != ';' && last != '}') continue;

        std::string full = session + pending;
        try {
            Lexer lex(full);
            auto tokens = lex.scan();
            Parser parser(std::move(tokens));
            auto ast = parser.parse();
            Compiler compiler;
            auto [chunk, funcs, main_entry] = compiler.compile(ast);
            if (dump) disassemble(chunk, funcs, main_entry);
            VM vm;
            vm.run(chunk, funcs, main_entry, trace);
            session = full;
            pending.clear();
        } catch (const std::exception& e) {
            std::cerr << "\033[31mError:\033[0m " << e.what() << "\n";
            pending.clear();
        }
    }
    std::cout << "\n  bye!\n";
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc == 1) {
        run_repl();
        return 0;
    }

    std::string path;
    bool dump  = false;
    bool trace = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--debug"  || arg == "-d") dump  = true;
        else if (arg == "--trace" || arg == "-t") trace = true;
        else                                       path  = arg;
    }

    if (path.empty()) {
        std::cerr << "Usage: cvmpp [script.cvm] [--debug] [--trace]\n";
        return 1;
    }

    return run_file(path, dump, trace);
}
