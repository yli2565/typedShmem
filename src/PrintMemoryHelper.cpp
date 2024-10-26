#include "PrintMemoryHelper.h"

void printMemoryDetailed(const void *ptr, size_t range)
{
    const char *charPtr = reinterpret_cast<const char *>(ptr);

    // Print header
    std::cerr << "Memory dump around " << ptr << "\n";
    std::cerr << "Offset    Hex     Char    Binary\n";
    std::cerr << "----------------------------------------\n";

    // Print before and after ptr
    for (int i = -static_cast<int>(range); i < static_cast<int>(range); i++)
    {
        unsigned char byte = static_cast<unsigned char>(charPtr[i]);

        // Print offset
        std::cerr << std::dec << std::setw(5) << i << "    ";

        // Print hex
        std::cerr << "0x" << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte) << "    ";

        // Print char (or dot if not printable)
        std::cerr << " " << (isprint(byte) ? static_cast<char>(byte) : '.') << "     ";

        // Print binary
        std::cerr << std::bitset<8>(byte) << "\n";
    }

    std::cerr << std::dec; // Reset to decimal
}