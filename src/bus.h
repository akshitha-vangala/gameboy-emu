#pragma once
#include <array>
#include <cstdint>

class Bus {
public:
    uint8_t read(uint16_t addr) const { return mem[addr]; }
    void write(uint16_t addr, uint8_t val) { mem[addr] = val; }
private:
    std::array<uint8_t, 0x10000> mem{};
};