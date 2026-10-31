#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Cartridge {
public:
    // Loads a .gb file and parses its header. Returns false on failure.
    bool load(const std::string& path);

    // Same, but from bytes already in memory (used by unit tests).
    // If save_file is non-empty and the cartridge has a battery, RAM is
    // loaded from / saved to that file.
    bool load_from_memory(std::vector<uint8_t> data, const std::string& save_file = "");

    void print_info() const;

    // Battery-backed RAM: writes <rom>.sav. Returns true if a file was written.
    bool save_ram() const;

    // Used by the bus
    uint8_t read_rom(uint16_t addr) const;          // 0000-7FFF
    void write_rom(uint16_t addr, uint8_t v);       // MBC control registers
    uint8_t read_ram(uint16_t addr) const;          // A000-BFFF
    void write_ram(uint16_t addr, uint8_t v);

    // Parsed header fields
    std::string title;
    uint8_t cgb_flag = 0;
    uint8_t type = 0;
    uint8_t rom_code = 0;
    uint8_t ram_code = 0;
    bool header_ok = false;   // header checksum matched
    bool battery = false;

    size_t rom_size() const { return rom.size(); }
    static const char* type_name(uint8_t t);

private:
    enum class Mbc { None, Mbc1, Unsupported };
    Mbc mbc = Mbc::None;

    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;
    std::string save_path;

    // MBC1 state
    bool ram_enabled = false;
    uint8_t bank1 = 1;      // 5-bit ROM bank register
    uint8_t bank2 = 0;      // 2-bit register
    bool mode = false;      // banking mode select

    size_t ram_offset(uint16_t addr) const;
};