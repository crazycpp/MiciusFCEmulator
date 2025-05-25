#include "Cartridge.h"
#include <fstream>
#include <iostream>

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

    std::cout << "[Cartridge] ROM Header Info:" << std::endl;
    std::cout << "  PRG-ROM Size: " << int(m_PrgRomSize) << " x 16KB = " << (m_PrgRomSize * 16) << "KB" << std::endl;
    std::cout << "  CHR-ROM Size: " << int(m_ChrRomSize) << " x 8KB = " << (m_ChrRomSize * 8) << "KB" << std::endl;
    std::cout << "  Mapper: " << int(m_Mapper) << std::endl;
    std::cout << "  Vertical Mirror: " << (m_VerticalMirror ? "Yes" : "No") << std::endl;

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
    
    // 处理CHR内存：CHR ROM大小为0表示使用CHR-RAM
    if (m_ChrRomSize == 0) {
        // CHR-RAM: 分配8KB的可写内存
        m_chrIsRam = true;
        m_ChrMemory.resize(0x2000);  // 8 KiB CHR-RAM
        // 初始化为0（这很重要，因为测试ROM会依赖初始状态）
        std::fill(m_ChrMemory.begin(), m_ChrMemory.end(), 0);
        std::cout << "[Cartridge] Using CHR-RAM, size=" << m_ChrMemory.size() << " bytes" << std::endl;
    } else {
        // CHR-ROM: 从文件加载只读数据
        m_chrIsRam = false;
        m_ChrMemory.resize(m_ChrRomSize * 8 * 1024);
        std::cout << "[Cartridge] Using CHR-ROM, size=" << m_ChrMemory.size() << " bytes" << std::endl;
    }

    auto prgRomBegin = romData.begin() + 16;

    // load PRG ROM
    std::copy(prgRomBegin, prgRomBegin + m_PrgMemory.size(), m_PrgMemory.begin());
    std::cout << "[Cartridge] Loaded PRG-ROM, size=" << m_PrgMemory.size() << " bytes" << std::endl;

    // load CHR ROM (只有当不是CHR-RAM时才从文件加载)
    if (!m_chrIsRam && m_ChrMemory.size() > 0) {
        auto chrRomBegin = prgRomBegin + m_PrgMemory.size();
        std::copy(chrRomBegin, chrRomBegin + m_ChrMemory.size(), m_ChrMemory.begin());
        std::cout << "[Cartridge] Loaded CHR-ROM, size=" << m_ChrMemory.size() << " bytes" << std::endl;
        
        // 显示CHR数据的前几个字节
        std::cout << "[Cartridge] CHR Data[0-15]: ";
        for (int i = 0; i < 16 && i < m_ChrMemory.size(); i++) {
            std::cout << std::hex << int(m_ChrMemory[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }
}

void Cartridge::WriteChrMemory(uint16_t addr, uint8_t data)
{
    if (m_chrIsRam && !m_ChrMemory.empty()) {
        // 8 KiB CHR-RAM地址环绕
        m_ChrMemory[addr & 0x1FFF] = data;
    }
    // CHR-ROM情况下什么都不做（只读）
}
