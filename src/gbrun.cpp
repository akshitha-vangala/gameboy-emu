// gbrun: load a .gb ROM, run the CPU headlessly, and show serial output.
// Usage: gbrun <rom.gb> [max_cycles]
#include <cstdio>
#include <cstdlib>
#include <string>
#include "bus.h"
#include "cartridge.h"
#include "cpu.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: gbrun <rom.gb> [max_cycles]\n");
        return 1;
    }

    Cartridge cart;
    if (!cart.load(argv[1])) return 1;
    cart.print_info();

    Bus bus;
    bus.attach(&cart);
    CPU cpu(bus);

    uint64_t limit = (argc > 2) ? std::strtoull(argv[2], nullptr, 10) : 200000000ULL;
    uint64_t cycles = 0;
    size_t seen = 0;

    std::printf("\n--- running ---\n");
    while (cycles < limit) {
        cycles += cpu.step();

        // Stop once a Blargg-style test reports a result over serial
        const std::string& s = bus.serial_output();
        if (s.size() != seen) {
            seen = s.size();
            if (s.find("Passed") != std::string::npos ||
                s.find("Failed") != std::string::npos) break;
        }
    }
    std::printf("\n--- stopped after %llu cycles ---\n", (unsigned long long)cycles);
    cart.save_ram();   // writes <rom>.sav if the cartridge has a battery
    return 0;
}