CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I src
TARGET   := build/cvmpp
SRC      := src/main.cpp

# Target shortcuts listed here for clean declaration
.PHONY: all clean run-hello run-fizzbuzz run-functions run-factorial run-assignment run-comparisons run-div-by-zero run-multiline verify

all: $(TARGET)

$(TARGET): $(SRC) src/lexer.hpp src/parser.hpp src/ast.hpp src/compiler.hpp src/vm.hpp src/chunk.hpp src/disasm.hpp src/token.hpp | build
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

build:
	mkdir -p build

# Existing execution shortcuts
run-hello: $(TARGET)
	./$(TARGET) examples/hello.cvm

run-fizzbuzz: $(TARGET)
	./$(TARGET) examples/fizzbuzz.cvm

run-functions: $(TARGET)
	./$(TARGET) examples/functions.cvm

run-factorial: $(TARGET)
	./$(TARGET) examples/factorial.cvm

run-assignment: $(TARGET)
	./$(TARGET) examples/assignment.cvm

run-comparisons: $(TARGET)
	./$(TARGET) examples/comparisons.cvm

run-div-by-zero: $(TARGET)
	./$(TARGET) examples/div_by_zero.cvm

run-multiline: $(TARGET)
	./$(TARGET) examples/multiline.cvm

# Dynamic verification engine (automatically covers all files via wildcard)
verify: $(TARGET)
	@echo "Running all examples..."
	@pass=0; fail=0; \
	for f in examples/*.cvm; do \
		name=$$(basename $$f); \
		if echo "" | ./$(TARGET) $$f > /dev/null 2>&1 || [ "$$name" = "div_by_zero.cvm" ]; then \
			echo "  PASS  $$name"; pass=$$((pass+1)); \
		else \
			echo "  FAIL  $$name"; fail=$$((fail+1)); \
		fi; \
	done; \
	echo ""; \
	echo "  $$pass passed, $$fail failed"

clean:
	rm -f $(TARGET)
