// CPU-only codec validation utility. Writes raw, tightly packed RGBA8.
#include "dds_decoder.h"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char** argv) {
    if(argc!=3) { std::cerr << "usage: dds-decode-file input.dds output.rgba\n"; return 2; }
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    if(!input || input.tellg()<0 || input.tellg()>128*1024*1024) return 2;
    const auto length=static_cast<std::size_t>(input.tellg());
    input.seekg(0);
    std::vector<std::uint8_t> bytes(length);
    if(length && !input.read(reinterpret_cast<char*>(bytes.data()),length)) return 2;
    const auto result=orbiter::dds::decode(bytes.data(),bytes.size());
    if(!result) { std::cerr << "DDS decode error " << static_cast<int>(result.error) << '\n'; return 1; }
    std::ofstream output(argv[2],std::ios::binary);
    output.write(reinterpret_cast<const char*>(result.image.rgba.data()),result.image.rgba.size());
    if(!output) return 2;
    std::cout << result.image.width << ' ' << result.image.height << '\n';
}
