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

    span<const uint8_t> GetPrgMemory()const { return m_PrgMemory; }
    span<const uint8_t> GetChrMemory()const{ return m_ChrMemory; }

private:
    bool CheckRomHeader(const std::vector<uint8_t>& romData);
    void LoadRomData(span<const uint8_t> romData);

private:
    
    filesystem::path m_FilePath;

    int m_PrgRomSize = 0;
    std::vector<uint8_t> m_PrgMemory;
    int m_ChrRomSize = 0;
    std::vector<uint8_t> m_ChrMemory;
    bool m_VerticalMirror = false;
    uint8_t m_Mapper = 0;

};