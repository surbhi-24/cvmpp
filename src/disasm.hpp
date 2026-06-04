#pragma once
#include "chunk.hpp"
#include "vm.hpp"    // for op_name... we duplicate the name table here for independence
#include <iostream>
#include <iomanip>
#include <string>

static void disassemble(const Chunk& chunk, const std::vector<FuncProto>& funcs, size_t main_entry) {
    auto op_str = [](Op op) -> std::string {
        switch (op) {
            case Op::PUSH_NUM:     return "PUSH_NUM";
            case Op::PUSH_BOOL:    return "PUSH_BOOL";
            case Op::PUSH_STR:     return "PUSH_STR";
            case Op::LOAD_GLOBAL:  return "LOAD_GLOBAL";
            case Op::STORE_GLOBAL: return "STORE_GLOBAL";
            case Op::LOAD_LOCAL:   return "LOAD_LOCAL";
            case Op::STORE_LOCAL:  return "STORE_LOCAL";
            case Op::ADD:  return "ADD";
            case Op::SUB:  return "SUB";
            case Op::MUL:  return "MUL";
            case Op::DIV:  return "DIV";
            case Op::MOD:  return "MOD";
            case Op::EQ:   return "EQ";
            case Op::NE:   return "NE";
            case Op::LT:   return "LT";
            case Op::GT:   return "GT";
            case Op::LE:   return "LE";
            case Op::GE:   return "GE";
            case Op::JUMP:       return "JUMP";
            case Op::JUMP_FALSE: return "JUMP_FALSE";
            case Op::CALL:       return "CALL";
            case Op::RETURN:     return "RETURN";
            case Op::PRINT:      return "PRINT";
            case Op::INPUT:       return "READ";
            case Op::POP:        return "POP";
            case Op::HALT:       return "HALT";
            default:             return "???";
        }
    };

    std::cout << "═══════════════════════════════════════\n";
    std::cout << "  Bytecode Disassembly  (" << chunk.code.size() << " bytes)\n";
    std::cout << "═══════════════════════════════════════\n";

    // Label function entry points
    auto func_label = [&](size_t addr) -> std::string {
        for (auto& f : funcs)
            if (f.entry == addr) return "<func " + f.name + ">";
        if (addr == main_entry) return "<main>";
        return "";
    };

    size_t pc = 0;
    while (pc < chunk.code.size()) {
        // Print section label
        std::string lbl = func_label(pc);
        if (!lbl.empty())
            std::cout << "\n  " << lbl << "\n";

        std::cout << "  " << std::setw(4) << std::setfill('0') << pc
                  << std::setfill(' ') << "  ";

        Op op = static_cast<Op>(chunk.code[pc++]);
        std::cout << std::left << std::setw(16) << op_str(op);

        switch (op) {
            case Op::PUSH_NUM: {
                int64_t v = chunk.read_i64(pc); pc += 8;
                std::cout << v;
                break;
            }
            case Op::PUSH_BOOL: {
                bool b = chunk.code[pc++] != 0;
                std::cout << (b ? "true" : "false");
                break;
            }
            case Op::PUSH_STR: {
                uint16_t idx = chunk.read_u16(pc); pc += 2;
                std::cout << "\"" << chunk.strings.at(idx) << "\"";
                break;
            }
            case Op::LOAD_GLOBAL:
            case Op::STORE_GLOBAL: {
                uint16_t idx = chunk.read_u16(pc); pc += 2;
                std::cout << chunk.names.at(idx) << "  (idx=" << idx << ")";
                break;
            }
            case Op::LOAD_LOCAL:
            case Op::STORE_LOCAL: {
                uint8_t slot = chunk.code[pc++];
                std::cout << "slot " << (int)slot;
                break;
            }
            case Op::JUMP:
            case Op::JUMP_FALSE: {
                uint32_t addr = chunk.read_u32(pc); pc += 4;
                std::cout << "-> " << addr;
                break;
            }
            case Op::CALL: {
                uint16_t idx = chunk.read_u16(pc); pc += 2;
                std::cout << funcs.at(idx).name << "  (idx=" << idx << ")";
                break;
            }
            default: break;
        }
        std::cout << "\n";
    }
    std::cout << "═══════════════════════════════════════\n\n";
}
