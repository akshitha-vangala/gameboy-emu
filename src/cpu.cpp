#include "cpu.h"
#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------
// Register access by 3-bit code (6 = (HL))
// ---------------------------------------------------------------
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

// 16-bit register pair by 2-bit code: 0=BC 1=DE 2=HL 3=SP
uint16_t CPU::get_rr(int code) const {
    switch (code) {
        case 0: return reg.bc();
        case 1: return reg.de();
        case 2: return reg.hl();
        default: return reg.sp;
    }
}

void CPU::set_rr(int code, uint16_t v) {
    switch (code) {
        case 0: reg.set_bc(v); break;
        case 1: reg.set_de(v); break;
        case 2: reg.set_hl(v); break;
        default: reg.sp = v; break;
    }
}

// ---------------------------------------------------------------
// ALU helpers
// ---------------------------------------------------------------
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
        case 0:    // ADD
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
        case 2:    // SUB
        case 3:    // SBC
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

// Shared by ADD SP,e8 and LD HL,SP+e8
uint16_t CPU::add_sp_e8(int8_t off) {
    uint16_t sp = reg.sp;
    uint16_t res = (uint16_t)(sp + off);
    reg.f = 0;
    reg.set_flag(FLAG_H, ((sp & 0x0F) + (off & 0x0F)) > 0x0F);
    reg.set_flag(FLAG_C, ((sp & 0xFF) + (off & 0xFF)) > 0xFF);
    return res;
}

// ---------------------------------------------------------------
// Stack and conditions
// ---------------------------------------------------------------
void CPU::push16(uint16_t v) {
    bus.write(--reg.sp, v >> 8);
    bus.write(--reg.sp, v & 0xFF);
}

uint16_t CPU::pop16() {
    uint8_t lo = bus.read(reg.sp++);
    uint8_t hi = bus.read(reg.sp++);
    return (hi << 8) | lo;
}

bool CPU::cond(int cc) const {
    switch (cc) {
        case 0: return !reg.flag(FLAG_Z);  // NZ
        case 1: return  reg.flag(FLAG_Z);  // Z
        case 2: return !reg.flag(FLAG_C);  // NC
        default: return reg.flag(FLAG_C);  // C
    }
}

// ---------------------------------------------------------------
// CB-prefixed instructions
// ---------------------------------------------------------------

// Rotate/shift ops for the CB table. Sets all flags.
uint8_t CPU::cb_shift(int op, uint8_t v) {
    uint8_t c, r;
    switch (op) {
        case 0: c = v >> 7; r = (v << 1) | c; break;                             // RLC
        case 1: c = v & 1;  r = (v >> 1) | (c << 7); break;                      // RRC
        case 2: c = v >> 7; r = (v << 1) | (reg.flag(FLAG_C) ? 1 : 0); break;    // RL
        case 3: c = v & 1;  r = (v >> 1) | (reg.flag(FLAG_C) ? 0x80 : 0); break; // RR
        case 4: c = v >> 7; r = v << 1; break;                                   // SLA
        case 5: c = v & 1;  r = (v >> 1) | (v & 0x80); break;                    // SRA (keeps sign bit)
        case 6: c = 0;      r = (v << 4) | (v >> 4); break;                      // SWAP
        default: c = v & 1; r = v >> 1; break;                                   // SRL
    }
    reg.f = 0;
    reg.set_flag(FLAG_Z, r == 0);
    reg.set_flag(FLAG_C, c);
    return r;
}

int CPU::execute_cb(uint8_t opcode) {
    int x = opcode >> 6;
    int y = (opcode >> 3) & 7;
    int z = opcode & 7;
    bool mem = (z == 6);

    switch (x) {
        case 0:  // rotate / shift / swap
            set_r(z, cb_shift(y, get_r(z)));
            return mem ? 16 : 8;
        case 1: {  // BIT y, r
            uint8_t v = get_r(z);
            reg.set_flag(FLAG_Z, (v & (1 << y)) == 0);
            reg.set_flag(FLAG_N, false);
            reg.set_flag(FLAG_H, true);
            return mem ? 12 : 8;
        }
        case 2:  // RES y, r
            set_r(z, get_r(z) & ~(1 << y));
            return mem ? 16 : 8;
        default:  // SET y, r
            set_r(z, get_r(z) | (1 << y));
            return mem ? 16 : 8;
    }
}

// ---------------------------------------------------------------
// Fetch -> decode -> execute
// ---------------------------------------------------------------
int CPU::step() {
    bool enable_ime = ime_pending;  // EI takes effect after the NEXT instruction
    uint8_t opcode = fetch8();
    int cycles = execute(opcode);
    if (enable_ime) { ime = true; ime_pending = false; }
    return cycles;
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

    // JR cc, e8  (001cc000)
    if ((opcode & 0xE7) == 0x20) {
        int8_t off = (int8_t)fetch8();
        if (cond((opcode >> 3) & 3)) { reg.pc += off; return 12; }
        return 8;
    }

    // JP cc, a16  (110cc010)
    if ((opcode & 0xE7) == 0xC2) {
        uint16_t addr = fetch16();
        if (cond((opcode >> 3) & 3)) { reg.pc = addr; return 16; }
        return 12;
    }

    // CALL cc, a16  (110cc100)
    if ((opcode & 0xE7) == 0xC4) {
        uint16_t addr = fetch16();
        if (cond((opcode >> 3) & 3)) { push16(reg.pc); reg.pc = addr; return 24; }
        return 12;
    }

    // RET cc  (110cc000)
    if ((opcode & 0xE7) == 0xC0) {
        if (cond((opcode >> 3) & 3)) { reg.pc = pop16(); return 20; }
        return 8;
    }

    // INC rr (00rr0011) / DEC rr (00rr1011)
    if ((opcode & 0xCF) == 0x03) { int r = (opcode >> 4) & 3; set_rr(r, get_rr(r) + 1); return 8; }
    if ((opcode & 0xCF) == 0x0B) { int r = (opcode >> 4) & 3; set_rr(r, get_rr(r) - 1); return 8; }

    // ADD HL, rr (00rr1001)
    if ((opcode & 0xCF) == 0x09) {
        uint16_t hl = reg.hl(), v = get_rr((opcode >> 4) & 3);
        uint32_t res = hl + v;
        reg.set_flag(FLAG_N, false);
        reg.set_flag(FLAG_H, ((hl & 0x0FFF) + (v & 0x0FFF)) > 0x0FFF);
        reg.set_flag(FLAG_C, res > 0xFFFF);
        reg.set_hl(res & 0xFFFF);
        return 8;
    }

    // RST n  (11ttt111): call to address ttt*8
    if ((opcode & 0xC7) == 0xC7) {
        push16(reg.pc);
        reg.pc = opcode & 0x38;
        return 16;
    }

    // PUSH rr  (11rr0101): BC, DE, HL, AF
    if ((opcode & 0xCF) == 0xC5) {
        switch ((opcode >> 4) & 3) {
            case 0: push16(reg.bc()); break;
            case 1: push16(reg.de()); break;
            case 2: push16(reg.hl()); break;
            default: push16(reg.af()); break;
        }
        return 16;
    }

    // POP rr  (11rr0001)
    if ((opcode & 0xCF) == 0xC1) {
        uint16_t v = pop16();
        switch ((opcode >> 4) & 3) {
            case 0: reg.set_bc(v); break;
            case 1: reg.set_de(v); break;
            case 2: reg.set_hl(v); break;
            default: reg.set_af(v); break;  // set_af masks F's low nibble
        }
        return 12;
    }

    // ---- Individual opcodes ----
    switch (opcode) {
        case 0x00: return 4;  // NOP
        case 0xCB: return execute_cb(fetch8());  // CB prefix

        // 16-bit immediate loads
        case 0x01: reg.set_bc(fetch16()); return 12;  // LD BC,d16
        case 0x11: reg.set_de(fetch16()); return 12;  // LD DE,d16
        case 0x21: reg.set_hl(fetch16()); return 12;  // LD HL,d16
        case 0x31: reg.sp = fetch16();    return 12;  // LD SP,d16

        // Jumps / calls / returns
        case 0xC3: reg.pc = fetch16(); return 16;     // JP a16
        case 0x18: {                                  // JR e8
            int8_t off = (int8_t)fetch8();
            reg.pc += off;
            return 12;
        }
        case 0xE9: reg.pc = reg.hl(); return 4;       // JP HL
        case 0xCD: {                                  // CALL a16
            uint16_t addr = fetch16();
            push16(reg.pc);
            reg.pc = addr;
            return 24;
        }
        case 0xC9: reg.pc = pop16(); return 16;       // RET
        case 0xD9: reg.pc = pop16(); ime = true; return 16;  // RETI

        // ---- indirect loads ----
        case 0x02: bus.write(reg.bc(), reg.a); return 8;   // LD (BC),A
        case 0x12: bus.write(reg.de(), reg.a); return 8;   // LD (DE),A
        case 0x0A: reg.a = bus.read(reg.bc()); return 8;   // LD A,(BC)
        case 0x1A: reg.a = bus.read(reg.de()); return 8;   // LD A,(DE)
        case 0x22: bus.write(reg.hl(), reg.a); reg.set_hl(reg.hl() + 1); return 8;  // LD (HL+),A
        case 0x32: bus.write(reg.hl(), reg.a); reg.set_hl(reg.hl() - 1); return 8;  // LD (HL-),A
        case 0x2A: reg.a = bus.read(reg.hl()); reg.set_hl(reg.hl() + 1); return 8;  // LD A,(HL+)
        case 0x3A: reg.a = bus.read(reg.hl()); reg.set_hl(reg.hl() - 1); return 8;  // LD A,(HL-)

        case 0xEA: bus.write(fetch16(), reg.a); return 16;          // LD (a16),A
        case 0xFA: reg.a = bus.read(fetch16()); return 16;          // LD A,(a16)
        case 0xE0: bus.write(0xFF00 + fetch8(), reg.a); return 12;  // LDH (a8),A
        case 0xF0: reg.a = bus.read(0xFF00 + fetch8()); return 12;  // LDH A,(a8)
        case 0xE2: bus.write(0xFF00 + reg.c, reg.a); return 8;      // LD (C),A
        case 0xF2: reg.a = bus.read(0xFF00 + reg.c); return 8;      // LD A,(C)

        case 0x08: {  // LD (a16),SP
            uint16_t addr = fetch16();
            bus.write(addr, reg.sp & 0xFF);
            bus.write(addr + 1, reg.sp >> 8);
            return 20;
        }

        // ---- SP ops ----
        case 0xF9: reg.sp = reg.hl(); return 8;                         // LD SP,HL
        case 0xE8: reg.sp = add_sp_e8((int8_t)fetch8()); return 16;     // ADD SP,e8
        case 0xF8: reg.set_hl(add_sp_e8((int8_t)fetch8())); return 12;  // LD HL,SP+e8

        // ---- accumulator rotates (Z always 0) ----
        case 0x07: {  // RLCA
            uint8_t c = reg.a >> 7;
            reg.a = (reg.a << 1) | c;
            reg.f = 0; reg.set_flag(FLAG_C, c);
            return 4;
        }
        case 0x0F: {  // RRCA
            uint8_t c = reg.a & 1;
            reg.a = (reg.a >> 1) | (c << 7);
            reg.f = 0; reg.set_flag(FLAG_C, c);
            return 4;
        }
        case 0x17: {  // RLA
            uint8_t c = reg.a >> 7;
            reg.a = (reg.a << 1) | (reg.flag(FLAG_C) ? 1 : 0);
            reg.f = 0; reg.set_flag(FLAG_C, c);
            return 4;
        }
        case 0x1F: {  // RRA
            uint8_t c = reg.a & 1;
            reg.a = (reg.a >> 1) | (reg.flag(FLAG_C) ? 0x80 : 0);
            reg.f = 0; reg.set_flag(FLAG_C, c);
            return 4;
        }

        // ---- DAA, CPL, SCF, CCF ----
        case 0x27: {  // DAA: fix A into valid BCD after add/sub
            int a = reg.a;
            if (!reg.flag(FLAG_N)) {
                if (reg.flag(FLAG_C) || a > 0x99) { a += 0x60; reg.set_flag(FLAG_C, true); }
                if (reg.flag(FLAG_H) || (a & 0x0F) > 0x09) a += 0x06;
            } else {
                if (reg.flag(FLAG_C)) a -= 0x60;
                if (reg.flag(FLAG_H)) a -= 0x06;
            }
            reg.a = a & 0xFF;
            reg.set_flag(FLAG_Z, reg.a == 0);
            reg.set_flag(FLAG_H, false);
            return 4;
        }
        case 0x2F:  // CPL
            reg.a = ~reg.a;
            reg.set_flag(FLAG_N, true); reg.set_flag(FLAG_H, true);
            return 4;
        case 0x37:  // SCF
            reg.set_flag(FLAG_N, false); reg.set_flag(FLAG_H, false); reg.set_flag(FLAG_C, true);
            return 4;
        case 0x3F:  // CCF
            reg.set_flag(FLAG_N, false); reg.set_flag(FLAG_H, false);
            reg.set_flag(FLAG_C, !reg.flag(FLAG_C));
            return 4;

        // ---- misc ----
        case 0xF3: ime = false; ime_pending = false; return 4;  // DI
        case 0xFB: ime_pending = true; return 4;                // EI (delayed one instruction)
        case 0x10: fetch8(); return 4;                          // STOP (treated as NOP for now)

        default:
            std::fprintf(stderr, "Unimplemented opcode 0x%02X at PC=0x%04X\n",
                        opcode, (unsigned)(uint16_t)(reg.pc - 1));
            std::exit(1);
    }
}