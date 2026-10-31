#include "bus.h"
#include "cartridge.h"
#include "cpu.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL line %d: %s\n", __LINE__, #cond); std::exit(1); } } while (0)

int main() {
    // ---------------- Step 2: registers and flags ----------------
    {
        Bus bus;
        CPU cpu(bus);

        cpu.reg.set_bc(0x1234);
        CHECK(cpu.reg.b == 0x12 && cpu.reg.c == 0x34);
        CHECK(cpu.reg.bc() == 0x1234);

        cpu.reg.set_af(0xFFFF);
        CHECK(cpu.reg.f == 0xF0);          // low nibble forced to 0

        cpu.reg.set_flag(FLAG_Z, false);
        CHECK(!cpu.reg.flag(FLAG_Z));
        cpu.reg.set_flag(FLAG_C, true);
        CHECK(cpu.reg.flag(FLAG_C));
    }

    // ---------------- Step 3: fetch/decode/execute ----------------
    // Program: LD A,0x42 ; LD B,0xFF ; INC B ; JP 0x0150
    {
        Bus b2;
        CPU c2(b2);
        c2.reg.pc = 0x0100;
        uint8_t prog[] = {0x3E, 0x42, 0x06, 0xFF, 0x04, 0xC3, 0x50, 0x01};
        for (int i = 0; i < 8; i++) b2.write(0x0100 + i, prog[i]);

        CHECK(c2.step() == 8);  CHECK(c2.reg.a == 0x42);
        CHECK(c2.step() == 8);  CHECK(c2.reg.b == 0xFF);
        CHECK(c2.step() == 4);  CHECK(c2.reg.b == 0x00);
        CHECK(c2.reg.flag(FLAG_Z));
        CHECK(c2.reg.flag(FLAG_H));
        CHECK(!c2.reg.flag(FLAG_N));
        CHECK(c2.step() == 16); CHECK(c2.reg.pc == 0x0150);
    }

    // ---------------- Step 4: ALU ----------------
    {
        Bus b; CPU c(b);
        // LD A,0x3A ; ADD A,0xC6  -> 0x00, Z H C set
        uint8_t p[] = {0x3E, 0x3A, 0xC6, 0xC6};
        for (int i = 0; i < 4; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step(); c.step();
        CHECK(c.reg.a == 0x00);
        CHECK(c.reg.flag(FLAG_Z) && c.reg.flag(FLAG_H) && c.reg.flag(FLAG_C));
        CHECK(!c.reg.flag(FLAG_N));
    }
    {
        Bus b; CPU c(b);
        // LD A,0x10 ; SUB 0x01  -> 0x0F, N H set
        uint8_t p[] = {0x3E, 0x10, 0xD6, 0x01};
        for (int i = 0; i < 4; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step(); c.step();
        CHECK(c.reg.a == 0x0F);
        CHECK(c.reg.flag(FLAG_N) && c.reg.flag(FLAG_H));
        CHECK(!c.reg.flag(FLAG_Z) && !c.reg.flag(FLAG_C));
    }
    {
        Bus b; CPU c(b);
        // LD B,0x55 ; LD A,B ; XOR A  -> A=0, only Z set
        uint8_t p[] = {0x06, 0x55, 0x78, 0xAF};
        for (int i = 0; i < 4; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step(); c.step();
        CHECK(c.reg.a == 0x55);
        c.step();
        CHECK(c.reg.a == 0x00 && c.reg.f == FLAG_Z);
    }
    {
        Bus b; CPU c(b);
        // LD HL,0xC000 ; LD (HL),0x7F ; LD A,(HL)
        uint8_t p[] = {0x21, 0x00, 0xC0, 0x36, 0x7F, 0x7E};
        for (int i = 0; i < 6; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step();
        CHECK(c.step() == 12);
        CHECK(b.read(0xC000) == 0x7F);
        CHECK(c.step() == 8);
        CHECK(c.reg.a == 0x7F);
    }

    // ---------------- Step 5: control flow and stack ----------------
    // CALL / RET
    {
        Bus b; CPU c(b);
        uint8_t main_prog[] = {0xCD, 0x00, 0x02};         // CALL 0x0200
        uint8_t sub[] = {0x3E, 0x99, 0xC9};               // LD A,0x99 ; RET
        for (int i = 0; i < 3; i++) b.write(0x0100 + i, main_prog[i]);
        for (int i = 0; i < 3; i++) b.write(0x0200 + i, sub[i]);
        c.reg.pc = 0x0100;

        CHECK(c.step() == 24);
        CHECK(c.reg.pc == 0x0200 && c.reg.sp == 0xFFFC);
        CHECK(b.read(0xFFFD) == 0x01 && b.read(0xFFFC) == 0x03);  // return addr 0x0103
        CHECK(c.step() == 8 && c.reg.a == 0x99);
        CHECK(c.step() == 16);
        CHECK(c.reg.pc == 0x0103 && c.reg.sp == 0xFFFE);
    }
    // JR backward (infinite loop): JR -2
    {
        Bus b; CPU c(b);
        b.write(0x0100, 0x18); b.write(0x0101, 0xFE);
        c.reg.pc = 0x0100;
        CHECK(c.step() == 12);
        CHECK(c.reg.pc == 0x0100);
    }
    // JR NZ taken vs not taken
    {
        Bus b; CPU c(b);
        b.write(0x0100, 0x20); b.write(0x0101, 0x05);
        c.reg.pc = 0x0100;
        c.reg.set_flag(FLAG_Z, true);
        CHECK(c.step() == 8 && c.reg.pc == 0x0102);   // not taken

        c.reg.pc = 0x0100;
        c.reg.set_flag(FLAG_Z, false);
        CHECK(c.step() == 12 && c.reg.pc == 0x0107);  // taken
    }
    // PUSH / POP, including AF masking
    {
        Bus b; CPU c(b);
        uint8_t p[] = {0x01, 0xFF, 0x12,   // LD BC,0x12FF
                       0xC5,               // PUSH BC
                       0xF1};              // POP AF
        for (int i = 0; i < 5; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step();
        CHECK(c.step() == 16);
        CHECK(c.step() == 12);
        CHECK(c.reg.a == 0x12 && c.reg.f == 0xF0);    // low nibble forced to 0
    }

    // ---------------- Step 6: 16-bit, indirect loads, misc ----------------
    // INC BC, ADD HL,BC
    {
        Bus b; CPU c(b);
        uint8_t p[] = {0x01, 0xFF, 0x0F,   // LD BC,0x0FFF
                       0x03,               // INC BC -> 0x1000
                       0x21, 0x00, 0x0F,   // LD HL,0x0F00
                       0x09};              // ADD HL,BC -> 0x1F00
        for (int i = 0; i < 8; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step();
        CHECK(c.step() == 8 && c.reg.bc() == 0x1000);
        c.step();
        CHECK(c.step() == 8 && c.reg.hl() == 0x1F00);
        CHECK(!c.reg.flag(FLAG_H) && !c.reg.flag(FLAG_C));
    }
    // LD (HL+),A and LDH
    {
        Bus b; CPU c(b);
        uint8_t p[] = {0x21, 0x00, 0xC0,   // LD HL,0xC000
                       0x3E, 0x5A,         // LD A,0x5A
                       0x22,               // LD (HL+),A
                       0xE0, 0x80};        // LDH (0x80),A
        for (int i = 0; i < 8; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step(); c.step();
        CHECK(c.step() == 8);
        CHECK(b.read(0xC000) == 0x5A && c.reg.hl() == 0xC001);
        CHECK(c.step() == 12);
        CHECK(b.read(0xFF80) == 0x5A);
    }
    // RLCA, and DAA (0x45 + 0x38 = 0x7D -> BCD 0x83)
    {
        Bus b; CPU c(b);
        uint8_t p[] = {0x3E, 0x85, 0x07,        // LD A,0x85 ; RLCA -> 0x0B, C=1
                       0x3E, 0x45, 0xC6, 0x38,  // LD A,0x45 ; ADD A,0x38
                       0x27};                   // DAA -> 0x83
        for (int i = 0; i < 8; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step(); c.step();
        CHECK(c.reg.a == 0x0B && c.reg.flag(FLAG_C) && !c.reg.flag(FLAG_Z));
        c.step(); c.step();
        CHECK(c.reg.a == 0x7D);
        c.step();
        CHECK(c.reg.a == 0x83);
    }
    // ADD SP,e8 flags come from the low byte
    {
        Bus b; CPU c(b);
        b.write(0x0100, 0xE8); b.write(0x0101, 0x02);  // ADD SP,+2
        c.reg.pc = 0x0100;
        c.reg.sp = 0x00FF;
        CHECK(c.step() == 16);
        CHECK(c.reg.sp == 0x0101);
        CHECK(c.reg.flag(FLAG_H) && c.reg.flag(FLAG_C));
        CHECK(!c.reg.flag(FLAG_Z) && !c.reg.flag(FLAG_N));
    }

    // ---------------- Step 7: CB-prefixed ----------------
    // RLC B, SWAP A, SRA A
    {
        Bus b; CPU c(b);
        uint8_t p[] = {0x06, 0x85, 0xCB, 0x00,    // LD B,0x85 ; RLC B -> 0x0B, C=1
                       0x3E, 0xF0, 0xCB, 0x37,    // LD A,0xF0 ; SWAP A -> 0x0F
                       0x3E, 0x81, 0xCB, 0x2F};   // LD A,0x81 ; SRA A -> 0xC0, C=1
        for (int i = 0; i < 12; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step();
        CHECK(c.step() == 8);
        CHECK(c.reg.b == 0x0B && c.reg.flag(FLAG_C) && !c.reg.flag(FLAG_Z));
        c.step();
        c.step();
        CHECK(c.reg.a == 0x0F && c.reg.f == 0);
        c.step();
        c.step();
        CHECK(c.reg.a == 0xC0 && c.reg.flag(FLAG_C));
    }
    // BIT 7,H
    {
        Bus b; CPU c(b);
        uint8_t p[] = {0x26, 0x80, 0xCB, 0x7C};   // LD H,0x80 ; BIT 7,H
        for (int i = 0; i < 4; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step();
        CHECK(c.step() == 8);
        CHECK(!c.reg.flag(FLAG_Z) && c.reg.flag(FLAG_H) && !c.reg.flag(FLAG_N));
    }
    // SET 0,(HL) ; BIT 0,(HL) ; RES 0,(HL)
    {
        Bus b; CPU c(b);
        uint8_t p[] = {0x21, 0x00, 0xC0,   // LD HL,0xC000
                       0xCB, 0xC6,         // SET 0,(HL)
                       0xCB, 0x46,         // BIT 0,(HL)
                       0xCB, 0x86};        // RES 0,(HL)
        for (int i = 0; i < 9; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step();
        CHECK(c.step() == 16 && b.read(0xC000) == 0x01);
        CHECK(c.step() == 12 && !c.reg.flag(FLAG_Z));
        CHECK(c.step() == 16 && b.read(0xC000) == 0x00);
    }

    // ---------------- Step 8: memory map ----------------
    {
        Bus b;
        // Echo RAM mirrors work RAM in both directions
        b.write(0xC123, 0xAB);
        CHECK(b.read(0xE123) == 0xAB);
        b.write(0xE200, 0x77);
        CHECK(b.read(0xC200) == 0x77);

        // VRAM, OAM, high RAM, interrupt enable
        b.write(0x8000, 0x11);  CHECK(b.read(0x8000) == 0x11);
        b.write(0x9FFF, 0x12);  CHECK(b.read(0x9FFF) == 0x12);
        b.write(0xFE00, 0x22);  CHECK(b.read(0xFE00) == 0x22);
        b.write(0xFF80, 0x44);  CHECK(b.read(0xFF80) == 0x44);
        b.write(0xFFFE, 0x45);  CHECK(b.read(0xFFFE) == 0x45);
        b.write(0xFFFF, 0x1F);  CHECK(b.read(0xFFFF) == 0x1F);

        // Unusable region: writes ignored, reads give 0xFF
        b.write(0xFEA0, 0x33);
        CHECK(b.read(0xFEA0) == 0xFF);

        // Cartridge RAM area with no cartridge reads as 0xFF
        CHECK(b.read(0xA000) == 0xFF);

        // Serial port: write a byte, start transfer, check it was captured
        b.echo_serial = false;
        b.write(0xFF01, 'H');
        b.write(0xFF02, 0x81);
        b.write(0xFF01, 'i');
        b.write(0xFF02, 0x81);
        CHECK(b.serial_output() == "Hi");
    }

    // ---------------- Step 9: MBC1 banking and cartridge RAM ----------------
    {
        // Fake 256 KB ROM (16 banks). Byte at offset 0x1000 of each bank = bank number.
        std::vector<uint8_t> data(16 * 0x4000, 0);
        for (int bank = 0; bank < 16; bank++) data[bank * 0x4000 + 0x1000] = (uint8_t)bank;
        data[0x147] = 0x03;   // MBC1+RAM+BATTERY
        data[0x148] = 0x03;   // 256 KB
        data[0x149] = 0x02;   // 8 KB RAM

        Cartridge cart;
        CHECK(cart.load_from_memory(data));   // no save file
        Bus b; b.attach(&cart);

        CHECK(b.read(0x1000) == 0);           // fixed bank 0
        CHECK(b.read(0x5000) == 1);           // default switchable bank is 1

        b.write(0x2000, 5);                   // select bank 5
        CHECK(b.read(0x5000) == 5);
        CHECK(b.read(0x1000) == 0);           // bank 0 area unchanged

        b.write(0x2000, 0);                   // bank 0 request becomes bank 1
        CHECK(b.read(0x5000) == 1);

        b.write(0x2000, 0x15);                // 21 wraps around 16 banks -> bank 5
        CHECK(b.read(0x5000) == 5);

        // Cartridge RAM: disabled by default, enabled by writing 0x0A to 0000-1FFF
        b.write(0xA000, 0x42);
        CHECK(b.read(0xA000) == 0xFF);        // disabled: write ignored, read gives FF
        b.write(0x0000, 0x0A);
        b.write(0xA000, 0x42);
        CHECK(b.read(0xA000) == 0x42);
        b.write(0x0000, 0x00);
        CHECK(b.read(0xA000) == 0xFF);        // disabled again
        b.write(0x0000, 0x0A);
        CHECK(b.read(0xA000) == 0x42);        // data survived being disabled

        // ROM-only cartridge: bank-switch writes do nothing
        std::vector<uint8_t> small(0x8000, 0);
        small[0x4000 + 0x10] = 0x99;
        Cartridge plain;
        CHECK(plain.load_from_memory(small));
        Bus b2; b2.attach(&plain);
        b2.write(0x2000, 3);
        CHECK(b2.read(0x4010) == 0x99);
    }

    std::puts("All CPU tests passed.");
    return 0;
}