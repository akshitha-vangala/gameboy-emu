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

    std::puts("All CPU tests passed.");
}