#include "Cartridge.h"
#include <fstream>

Cartridge::Cartridge(const filesystem::path& filePath)
    : m_FilePath(filePath)
{
    
}

bool Cartridge::Load()
{
    if (!std::filesystem::exists(m_FilePath))
    {
        // add log or message box here
        return false;
    }

    std::ifstream file(m_FilePath, std::ios::binary);
    if (!file.is_open())
    {
        // add log or message box here
        return false;
    }

    std::vector<uint8_t> romData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (!CheckRomHeader(romData))
    {
        // add log or message box here
        return false;
    }

    m_PrgRomSize = romData[4];
    m_ChrRomSize = romData[5];
    m_Mapper = (romData[6] >> 4) | (romData[7] & 0xF0);
    m_VerticalMirror = (romData[6] & 0x01) != 0;

    LoadRomData(romData);

    return true;
}

bool Cartridge::CheckRomHeader(const std::vector<uint8_t>& romData)
{
    // nes file header is 16 bytes
    // 0-3: Constant $4E $45 $53 $1A ("NES" followed by 0x1A)
    // 4: Size of PRG ROM in 16 KB units
    // 5: Size of CHR ROM in 8 KB units (Value 0 means the board uses CHR RAM)
    // 6: Flags 6
    // 7: Flags 7
    // 8: Size of PRG RAM in 8 KB units (Value 0 infers 8 KB for compatibility; see PRG RAM circuit)
    // 9: Flags 9
    // 10: Flags 10 (unofficial)
    // 11-15: Zero filled

    if (romData.size() < 16)
    {
        // add log or message box here
        return false;
    }

    // the first 4 bytes of the ROM should be "NES"  followed by 0x1A
    if (romData[0] != 'N' || romData[1] != 'E' || romData[2] != 'S' || romData[3] != 0x1A)
    {
        // add log or message box here
        return false;
    }

    return true;
}

void Cartridge::LoadRomData(const std::vector<uint8_t>& romData)
{
    m_PrgMemory.resize(m_PrgRomSize * 16 * 1024);
    m_ChrMemory.resize(m_ChrRomSize * 8 * 1024);

    auto prgRomBegin = romData.begin() + 16;

    // load PRG ROM
    std::copy(prgRomBegin, prgRomBegin + m_PrgMemory.size(), m_PrgMemory.begin());

    // load CHR ROM
    auto chrRomBegin = prgRomBegin + m_PrgMemory.size();
    std::copy(chrRomBegin, chrRomBegin + m_ChrMemory.size(), m_ChrMemory.begin());
}
