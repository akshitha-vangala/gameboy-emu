#pragma once
#include <cstdint>

enum Flag : uint8_t {
    FLAG_Z = 0x80,
    FLAG_N = 0x40,
    FLAG_H = 0x20,
    FLAG_C = 0x10,
};

struct Registers {
    // Values after the DMG boot ROM finishes
    uint8_t a = 0x01, f = 0xB0;
    uint8_t b = 0x00, c = 0x13;
    uint8_t d = 0x00, e = 0xD8;
    uint8_t h = 0x01, l = 0x4D;
    uint16_t sp = 0xFFFE;
    uint16_t pc = 0x0100;

    // 16-bit pair accessors
    uint16_t af() const { return (a << 8) | f; }
    uint16_t bc() const { return (b << 8) | c; }
    uint16_t de() const { return (d << 8) | e; }
    uint16_t hl() const { return (h << 8) | l; }

    void set_af(uint16_t v) { a = v >> 8; f = v & 0xF0; }  // low nibble of F stays 0
    void set_bc(uint16_t v) { b = v >> 8; c = v & 0xFF; }
    void set_de(uint16_t v) { d = v >> 8; e = v & 0xFF; }
    void set_hl(uint16_t v) { h = v >> 8; l = v & 0xFF; }

    // Flag helpers
    bool flag(Flag fl) const { return (f & fl) != 0; }
    void set_flag(Flag fl, bool on) {
        if (on) f |= fl; else f &= ~fl;
    }
};