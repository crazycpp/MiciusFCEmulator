#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <span>

using namespace std;

class Cartridge
 {

public:
    Cartridge(const filesystem::path& filePath);
    ~Cartridge() = default;

    bool Load();

    // 提供std::span访问内存
    //std::span<const uint8_t> GetPrgRom() const { return std::span<const uint8_t>(m_PrgMemory.data(), m_PrgMemory.size()); }
    //std::span<const uint8_t> GetChrRom() const { return std::span<const uint8_t>(m_ChrMemory.data(), m_ChrMemory.size()); }
    
    // 同时保留原始访问方式以保证兼容性
    const uint8_t* GetPrgMemory() const { return m_PrgMemory.data(); }
    size_t GetPrgMemorySize() const { return m_PrgMemory.size(); }
    
    const uint8_t* GetChrMemory() const { return m_ChrMemory.data(); }
    size_t GetChrMemorySize() const { return m_ChrMemory.size(); }

private:
    bool CheckRomHeader(const std::vector<uint8_t>& romData);
    void LoadRomData(const std::vector<uint8_t>& romData);

private:
    
    filesystem::path m_FilePath;

    int m_PrgRomSize = 0;
    std::vector<uint8_t> m_PrgMemory;
    int m_ChrRomSize = 0;
    std::vector<uint8_t> m_ChrMemory;
    bool m_VerticalMirror = false;
    uint8_t m_Mapper = 0;

};