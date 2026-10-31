#pragma once
#include "bus.h"
#include "registers.h"

class CPU {
public:
    explicit CPU(Bus& bus) : bus(bus) {}
    Registers reg;
private:
    Bus& bus;
};