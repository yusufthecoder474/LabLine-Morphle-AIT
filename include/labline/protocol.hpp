#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
namespace labline {
using Bytes = std::vector<uint8_t>;
constexpr size_t max_payload = 64;
struct Frame {
    uint32_t sequence{};
    uint8_t device{}, operation{};
    Bytes payload;
};
uint32_t crc32(const uint8_t* p, size_t n);
void put32(Bytes& b, uint32_t v);
uint32_t get32(const Bytes& b, size_t at);
Bytes encode(const Frame& f);
class Decoder {
    Bytes buffer_;
public:
    size_t errors = 0;
    size_t crc_errors = 0;
    std::vector<Frame> feed(const uint8_t* p, size_t n);
    std::vector<Frame> feed(const Bytes& b) { return feed(b.data(), b.size()); }
};
}
