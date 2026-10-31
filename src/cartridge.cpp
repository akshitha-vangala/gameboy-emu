#include "cartridge.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <utility>

const char* Cartridge::type_name(uint8_t t) {
    switch (t) {
        case 0x00: return "ROM ONLY";
        case 0x01: return "MBC1";
        case 0x02: return "MBC1+RAM";
        case 0x03: return "MBC1+RAM+BATTERY";
        case 0x08: return "ROM+RAM";
        case 0x09: return "ROM+RAM+BATTERY";
        case 0x0F: return "MBC3+TIMER+BATTERY";
        case 0x10: return "MBC3+TIMER+RAM+BATTERY";
        case 0x11: return "MBC3";
        case 0x12: return "MBC3+RAM";
        case 0x13: return "MBC3+RAM+BATTERY";
        case 0x19: return "MBC5";
        case 0x1A: return "MBC5+RAM";
        case 0x1B: return "MBC5+RAM+BATTERY";
        case 0x1C: return "MBC5+RUMBLE";
        case 0x1D: return "MBC5+RUMBLE+RAM";
        case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
        default:   return "UNKNOWN / unsupported";
    }
}

static size_t ram_bytes(uint8_t code) {
    switch (code) {
        case 1: return 2 * 1024;     // unofficial
        case 2: return 8 * 1024;
        case 3: return 32 * 1024;
        case 4: return 128 * 1024;
        case 5: return 64 * 1024;
        default: return 0;
    }
}

static bool type_has_battery(uint8_t t) {
    switch (t) {
        case 0x03: case 0x09: case 0x0F: case 0x10:
        case 0x13: case 0x1B: case 0x1E: return true;
        default: return false;
    }
}

// "games/tetris.gb" -> "games/tetris.sav"
static std::string make_save_path(const std::string& rom_path) {
    size_t dot = rom_path.find_last_of('.');
    size_t slash = rom_path.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        return rom_path.substr(0, dot) + ".sav";
    return rom_path + ".sav";
}

bool Cartridge::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "Cannot open ROM file: %s\n", path.c_str());
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    return load_from_memory(std::move(data), make_save_path(path));
}

bool Cartridge::load_from_memory(std::vector<uint8_t> data, const std::string& save_file) {
    rom = std::move(data);

    if (rom.size() < 0x150) {
        std::fprintf(stderr, "File too small to be a Game Boy ROM (%zu bytes)\n", rom.size());
        return false;
    }

    // Title: 0134-0142, printable characters only
    title.clear();
    for (int i = 0x134; i <= 0x142; i++) {
        uint8_t ch = rom[i];
        if (ch == 0) break;
        if (ch >= 0x20 && ch < 0x7F) title.push_back((char)ch);
    }

    cgb_flag = rom[0x143];
    type     = rom[0x147];
    rom_code = rom[0x148];
    ram_code = rom[0x149];

    // Header checksum over 0134-014C, compared with the byte at 014D
    uint8_t x = 0;
    for (int i = 0x134; i <= 0x14C; i++) x = x - rom[i] - 1;
    header_ok = (x == rom[0x14D]);

    // Which memory bank controller?
    switch (type) {
        case 0x00: case 0x08: case 0x09: mbc = Mbc::None; break;
        case 0x01: case 0x02: case 0x03: mbc = Mbc::Mbc1; break;
        default:                         mbc = Mbc::Unsupported; break;
    }
    if (mbc == Mbc::Unsupported)
        std::fprintf(stderr, "Warning: cartridge type 0x%02X (%s) is not supported yet.\n",
                     type, type_name(type));
    if (mbc == Mbc::None && rom.size() > 0x8000)
        std::fprintf(stderr, "Warning: ROM is larger than 32 KB but has no MBC.\n");

    // Reset MBC state
    ram_enabled = false;
    bank1 = 1;
    bank2 = 0;
    mode = false;

    // Cartridge RAM, plus battery save if there is one
    ram.assign(ram_bytes(ram_code), 0);
    battery = type_has_battery(type);
    save_path = save_file;
    if (battery && !ram.empty() && !save_path.empty()) {
        std::ifstream sf(save_path, std::ios::binary);
        if (sf) {
            sf.read(reinterpret_cast<char*>(ram.data()), (std::streamsize)ram.size());
            std::printf("Loaded save file: %s\n", save_path.c_str());
        }
    }
    return true;
}

bool Cartridge::save_ram() const {
    if (!battery || ram.empty() || save_path.empty()) return false;
    std::ofstream sf(save_path, std::ios::binary);
    if (!sf) {
        std::fprintf(stderr, "Could not write save file: %s\n", save_path.c_str());
        return false;
    }
    sf.write(reinterpret_cast<const char*>(ram.data()), (std::streamsize)ram.size());
    std::printf("Saved cartridge RAM to %s\n", save_path.c_str());
    return true;
}

void Cartridge::print_info() const {
    size_t expected = (rom_code <= 8) ? ((size_t)32 * 1024 << rom_code) : 0;
    std::printf("Title:        %s\n", title.c_str());
    std::printf("Type:         0x%02X (%s)\n", type, type_name(type));
    std::printf("ROM size:     %zu bytes (header says %zu)\n", rom.size(), expected);
    std::printf("RAM size:     %zu bytes%s\n", ram.size(), battery ? " (battery-backed)" : "");
    std::printf("Game Boy Color flag: 0x%02X\n", cgb_flag);
    std::printf("Header check: %s\n", header_ok ? "OK" : "FAILED");
}

// ---------------------------------------------------------------
// ROM access (with MBC1 banking)
// ---------------------------------------------------------------
uint8_t Cartridge::read_rom(uint16_t addr) const {
    size_t offset = addr;

    if (mbc == Mbc::Mbc1) {
        size_t banks = rom.size() / 0x4000;
        if (banks == 0) banks = 1;

        if (addr < 0x4000) {
            // Bank 0 area. In mode 1 the upper bits can move it (large ROMs).
            size_t bank = mode ? ((size_t)bank2 << 5) : 0;
            offset = (bank % banks) * 0x4000 + addr;
        } else {
            size_t bank = ((size_t)bank2 << 5) | bank1;
            offset = (bank % banks) * 0x4000 + (addr - 0x4000);
        }
    }
    return offset < rom.size() ? rom[offset] : 0xFF;
}

void Cartridge::write_rom(uint16_t addr, uint8_t v) {
    if (mbc != Mbc::Mbc1) return;  // ROM-only: writes do nothing

    if (addr < 0x2000) {
        ram_enabled = ((v & 0x0F) == 0x0A);
    } else if (addr < 0x4000) {
        bank1 = v & 0x1F;
        if (bank1 == 0) bank1 = 1;         // bank 0 can't be selected here
    } else if (addr < 0x6000) {
        bank2 = v & 0x03;
    } else {
        mode = (v & 1) != 0;
    }
}

// ---------------------------------------------------------------
// Cartridge RAM
// ---------------------------------------------------------------
size_t Cartridge::ram_offset(uint16_t addr) const {
    size_t bank = (mbc == Mbc::Mbc1 && mode) ? bank2 : 0;
    return (bank * 0x2000 + (addr - 0xA000)) % ram.size();
}

uint8_t Cartridge::read_ram(uint16_t addr) const {
    if (ram.empty()) return 0xFF;
    if (mbc == Mbc::Mbc1 && !ram_enabled) return 0xFF;
    return ram[ram_offset(addr)];
}

void Cartridge::write_ram(uint16_t addr, uint8_t v) {
    if (ram.empty()) return;
    if (mbc == Mbc::Mbc1 && !ram_enabled) return;
    ram[ram_offset(addr)] = v;
}