#pragma once
#include <cstdint>
#include "bus.h"
#include "registers.h"

class CPU {
public:
    explicit CPU(Bus& bus) : bus(bus) {}
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

    // ALU helpers
    void alu(int op, uint8_t value);
    uint8_t inc8(uint8_t v);
    uint8_t dec8(uint8_t v);

    int execute(uint8_t opcode);
    bool halted = false;
};