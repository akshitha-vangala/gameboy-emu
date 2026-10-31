#include "bus.h"
#include "cartridge.h"
#include <cstdio>

Bus::Bus() {
    // A few I/O register values as left by the DMG boot ROM
    io[0x00] = 0xCF;  // P1 joypad
    io[0x0F] = 0xE1;  // IF  interrupt flags
    io[0x40] = 0x91;  // LCDC
    io[0x47] = 0xFC;  // BGP
}

uint8_t Bus::read(uint16_t a) const {
    if (a < 0x8000) return cart ? cart->read_rom(a) : flat_rom[a];
    if (a < 0xA000) return vram[a - 0x8000];
    if (a < 0xC000) return cart ? cart->read_ram(a) : 0xFF;
    if (a < 0xE000) return wram[a - 0xC000];
    if (a < 0xFE00) return wram[a - 0xE000];   // echo RAM
    if (a < 0xFEA0) return oam[a - 0xFE00];
    if (a < 0xFF00) return 0xFF;               // unusable
    if (a < 0xFF80) return io[a - 0xFF00];
    if (a < 0xFFFF) return hram[a - 0xFF80];
    return ie;
}

void Bus::write(uint16_t a, uint8_t v) {
    if (a < 0x8000) {
        if (cart) cart->write_rom(a, v);       // MBC control registers
        else flat_rom[a] = v;
        return;
    }
    if (a < 0xA000) { vram[a - 0x8000] = v; return; }
    if (a < 0xC000) { if (cart) cart->write_ram(a, v); return; }
    if (a < 0xE000) { wram[a - 0xC000] = v; return; }
    if (a < 0xFE00) { wram[a - 0xE000] = v; return; }  // echo RAM
    if (a < 0xFEA0) { oam[a - 0xFE00] = v; return; }
    if (a < 0xFF00) return;                    // unusable: writes ignored
    if (a < 0xFF80) {
        if (a == 0xFF02) {
            // Serial control: a transfer start (0x81) sends the byte in FF01.
            if ((v & 0x81) == 0x81) {
                char ch = (char)io[0x01];
                serial.push_back(ch);
                if (echo_serial) { std::putchar(ch); std::fflush(stdout); }
                io[0x02] = v & 0x7F;           // mark transfer complete
                return;
            }
        }
        io[a - 0xFF00] = v;
        return;
    }
    if (a < 0xFFFF) { hram[a - 0xFF80] = v; return; }
    ie = v;
}