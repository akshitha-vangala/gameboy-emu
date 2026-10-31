#include "cpu.h"
#include <cstdio>
#include <cstdlib>

#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL line %d: %s\n", __LINE__, #cond); std::exit(1); } } while (0)

int main() {
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
            // ALU tests
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
        // LD HL,0xC000 ; LD (HL),0x7F ; LD A,(HL) via LD A,(HL)=0x7E
        uint8_t p[] = {0x21, 0x00, 0xC0, 0x36, 0x7F, 0x7E};
        for (int i = 0; i < 6; i++) b.write(0x0100 + i, p[i]);
        c.reg.pc = 0x0100;
        c.step();
        CHECK(c.step() == 12);
        CHECK(b.read(0xC000) == 0x7F);
        CHECK(c.step() == 8);
        CHECK(c.reg.a == 0x7F);
    }

    std::puts("All CPU tests passed.");
}