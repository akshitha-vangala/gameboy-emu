#pragma once
#include <array>
#include <cstdint>
#include <string>

class Cartridge;

// The memory bus: routes every CPU read/write to the right hardware.
class Bus {
public:
    Bus();

    // Attach a loaded cartridge. Without one, the ROM area is a plain
    // writable array (handy for unit tests).
    void attach(Cartridge* c) { cart = c; }

    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t val);

    // Everything the game has sent out of the serial port (FF01/FF02).
    const std::string& serial_output() const { return serial; }
    bool echo_serial = true;  // also print serial bytes to stdout

private:
    Cartridge* cart = nullptr;

    std::array<uint8_t, 0x8000> flat_rom{};  // used only when no cartridge
    std::array<uint8_t, 0x2000> vram{};      // 8000-9FFF
    std::array<uint8_t, 0x2000> wram{};      // C000-DFFF
    std::array<uint8_t, 0xA0>   oam{};       // FE00-FE9F
    std::array<uint8_t, 0x80>   io{};        // FF00-FF7F
    std::array<uint8_t, 0x7F>   hram{};      // FF80-FFFE
    uint8_t ie = 0;                          // FFFF

    std::string serial;
};