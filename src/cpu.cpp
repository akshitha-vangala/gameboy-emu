#include "cpu.h"
#include <cstdio>
#include <cstdlib>

uint8_t CPU::get_r(int code) {
    switch (code) {
        case 0: return reg.b;
        case 1: return reg.c;
        case 2: return reg.d;
        case 3: return reg.e;
        case 4: return reg.h;
        case 5: return reg.l;
        case 6: return bus.read(reg.hl());
        default: return reg.a;
    }
}

void CPU::set_r(int code, uint8_t v) {
    switch (code) {
        case 0: reg.b = v; break;
        case 1: reg.c = v; break;
        case 2: reg.d = v; break;
        case 3: reg.e = v; break;
        case 4: reg.h = v; break;
        case 5: reg.l = v; break;
        case 6: bus.write(reg.hl(), v); break;
        default: reg.a = v; break;
    }
}

uint8_t CPU::inc8(uint8_t v) {
    uint8_t r = v + 1;
    reg.set_flag(FLAG_Z, r == 0);
    reg.set_flag(FLAG_N, false);
    reg.set_flag(FLAG_H, (v & 0x0F) == 0x0F);
    return r;  // C unaffected
}

uint8_t CPU::dec8(uint8_t v) {
    uint8_t r = v - 1;
    reg.set_flag(FLAG_Z, r == 0);
    reg.set_flag(FLAG_N, true);
    reg.set_flag(FLAG_H, (v & 0x0F) == 0x00);
    return r;  // C unaffected
}

// op: 0 ADD, 1 ADC, 2 SUB, 3 SBC, 4 AND, 5 XOR, 6 OR, 7 CP
void CPU::alu(int op, uint8_t v) {
    uint8_t a = reg.a;
    int carry = reg.flag(FLAG_C) ? 1 : 0;
    int res;

    switch (op) {
        case 0:  // ADD
        case 1: {  // ADC
            int c = (op == 1) ? carry : 0;
            res = a + v + c;
            reg.set_flag(FLAG_Z, (res & 0xFF) == 0);
            reg.set_flag(FLAG_N, false);
            reg.set_flag(FLAG_H, ((a & 0x0F) + (v & 0x0F) + c) > 0x0F);
            reg.set_flag(FLAG_C, res > 0xFF);
            reg.a = res & 0xFF;
            break;
        }
        case 2:  // SUB
        case 3:  // SBC
        case 7: {  // CP (like SUB but result is discarded)
            int c = (op == 3) ? carry : 0;
            res = a - v - c;
            reg.set_flag(FLAG_Z, (res & 0xFF) == 0);
            reg.set_flag(FLAG_N, true);
            reg.set_flag(FLAG_H, (a & 0x0F) < ((v & 0x0F) + c));
            reg.set_flag(FLAG_C, res < 0);
            if (op != 7) reg.a = res & 0xFF;
            break;
        }
        case 4:  // AND
            reg.a = a & v;
            reg.f = 0;
            reg.set_flag(FLAG_Z, reg.a == 0);
            reg.set_flag(FLAG_H, true);
            break;
        case 5:  // XOR
            reg.a = a ^ v;
            reg.f = 0;
            reg.set_flag(FLAG_Z, reg.a == 0);
            break;
        case 6:  // OR
            reg.a = a | v;
            reg.f = 0;
            reg.set_flag(FLAG_Z, reg.a == 0);
            break;
    }
}

int CPU::step() {
    uint8_t opcode = fetch8();
    return execute(opcode);
}

int CPU::execute(uint8_t opcode) {
    // ---- Structured groups first ----

    // HALT (must be checked before the LD r,r' block)
    if (opcode == 0x76) {
        halted = true;
        return 4;
    }

    // LD r, r'  (0x40-0x7F)
    if (opcode >= 0x40 && opcode <= 0x7F) {
        int dst = (opcode >> 3) & 7;
        int src = opcode & 7;
        set_r(dst, get_r(src));
        return (dst == 6 || src == 6) ? 8 : 4;
    }

    // ALU A, r  (0x80-0xBF)
    if (opcode >= 0x80 && opcode <= 0xBF) {
        int op = (opcode >> 3) & 7;
        int src = opcode & 7;
        alu(op, get_r(src));
        return (src == 6) ? 8 : 4;
    }

    // ALU A, d8  (0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE)
    if ((opcode & 0xC7) == 0xC6) {
        alu((opcode >> 3) & 7, fetch8());
        return 8;
    }

    // INC r  (00 yyy 100)
    if ((opcode & 0xC7) == 0x04) {
        int r = (opcode >> 3) & 7;
        set_r(r, inc8(get_r(r)));
        return (r == 6) ? 12 : 4;
    }

    // DEC r  (00 yyy 101)
    if ((opcode & 0xC7) == 0x05) {
        int r = (opcode >> 3) & 7;
        set_r(r, dec8(get_r(r)));
        return (r == 6) ? 12 : 4;
    }

    // LD r, d8  (00 yyy 110)
    if ((opcode & 0xC7) == 0x06) {
        int r = (opcode >> 3) & 7;
        set_r(r, fetch8());
        return (r == 6) ? 12 : 8;
    }

    // ---- Individual opcodes ----
    switch (opcode) {
        case 0x00: return 4;  // NOP

        case 0x01: reg.set_bc(fetch16()); return 12;  // LD BC,d16
        case 0x11: reg.set_de(fetch16()); return 12;  // LD DE,d16
        case 0x21: reg.set_hl(fetch16()); return 12;  // LD HL,d16
        case 0x31: reg.sp = fetch16();    return 12;  // LD SP,d16

        case 0xC3: reg.pc = fetch16(); return 16;     // JP a16

        default:
            std::fprintf(stderr, "Unimplemented opcode 0x%02X at PC=0x%04X\n",
                        opcode, reg.pc - 1);
            std::exit(1);
    }
}