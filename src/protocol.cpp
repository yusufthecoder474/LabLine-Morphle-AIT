#include "labline/protocol.hpp"
#include <algorithm>
namespace labline {
uint32_t crc32(const uint8_t* p, size_t n) {
    uint32_t c = 0xffffffffU;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int bit = 0; bit < 8; ++bit)
            c = (c >> 1) ^ (0xedb88320U & (0U - (c & 1U)));
    }
    return ~c;
}
void put32(Bytes& b, uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8) b.push_back(uint8_t(v >> shift));
}
uint32_t get32(const Bytes& b, size_t at) {
    if (at + 4 > b.size()) throw std::runtime_error("short integer");
    uint32_t v = 0;
    for (size_t i = at; i < at + 4; ++i) v = (v << 8) | b[i];
    return v;
}
Bytes encode(const Frame& f) {
    if (f.payload.size() > max_payload) throw std::runtime_error("oversize payload");
    Bytes b{0xa5, 1, 0, uint8_t(f.payload.size())};
    put32(b, f.sequence);
    b.push_back(f.device);
    b.push_back(f.operation);
    b.insert(b.end(), f.payload.begin(), f.payload.end());
    put32(b, crc32(b.data() + 1, b.size() - 1));
    b.push_back(0x5a);
    return b;
}
std::vector<Frame> Decoder::feed(const uint8_t* p, size_t n) {
    std::vector<Frame> out;
    // Parse incrementally: retained storage is bounded by one maximum frame.
    for (size_t i = 0; i < n; ++i) {
        buffer_.push_back(p[i]);
        for (;;) {
            auto start = std::find(buffer_.begin(), buffer_.end(), uint8_t(0xa5));
            buffer_.erase(buffer_.begin(), start);
            if (buffer_.size() < 4) break;
            size_t length = (size_t(buffer_[2]) << 8) | buffer_[3];
            if (buffer_[1] != 1 || length > max_payload) {
                ++errors;
                buffer_.erase(buffer_.begin());
                continue;
            }
            size_t total = 15 + length;
            if (buffer_.size() < total) break;
            bool crc_ok = get32(buffer_, 10 + length) == crc32(buffer_.data() + 1, 9 + length);
            if (!crc_ok || buffer_[total - 1] != 0x5a) {
                ++errors;
                if (!crc_ok) ++crc_errors;
                buffer_.erase(buffer_.begin());
                continue;
            }
            out.push_back({get32(buffer_, 4), buffer_[8], buffer_[9],
                           Bytes(buffer_.begin() + 10, buffer_.begin() + 10 + length)});
            buffer_.erase(buffer_.begin(), buffer_.begin() + total);
        }
    }
    return out;
}
}
