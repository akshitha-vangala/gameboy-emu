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

    std::puts("All CPU tests passed.");
}