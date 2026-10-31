#pragma once
#include <cstdint>
#include "bus.h"
#include "registers.h"

class CPU {
public:
    explicit CPU(Bus& bus) : bus(bus) {}

    // Runs one instruction, returns T-cycles used
    int step();

    Registers reg;

private:
    Bus& bus;

    uint8_t fetch8() { return bus.read(reg.pc++); }
    uint16_t fetch16() {
        uint8_t lo = fetch8();
        uint8_t hi = fetch8();
        return (hi << 8) | lo;
    }

    // Register access by 3-bit code (6 = (HL))
    uint8_t get_r(int code);
    void set_r(int code, uint8_t v);

    // 16-bit register pair by 2-bit code (0=BC 1=DE 2=HL 3=SP)
    uint16_t get_rr(int code) const;
    void set_rr(int code, uint16_t v);

    // ALU helpers
    void alu(int op, uint8_t value);
    uint8_t inc8(uint8_t v);
    uint8_t dec8(uint8_t v);
    uint16_t add_sp_e8(int8_t off);

    // Stack and conditions
    void push16(uint16_t v);
    uint16_t pop16();
    bool cond(int cc) const;

    int execute(uint8_t opcode);

    bool halted = false;
    bool ime = false;          // interrupt master enable
    bool ime_pending = false;  // EI takes effect after the next instruction
};