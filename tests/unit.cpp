#include "labline/core.hpp"
#include <iostream>
#include <thread>
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
int main(int argc, char** argv) {
    try {
        using namespace labline;
        std::string test = argc > 1 ? argv[1] : "";
        Frame frame{0x01020304, 2, 1, {0, 0, 0, 1}};
        auto bytes = encode(frame);
        Decoder d;
        if (test == "roundtrip") {
            auto v = d.feed(bytes); CHECK(v.size() == 1); CHECK(v[0].sequence == frame.sequence); CHECK(v[0].payload == frame.payload);
            const std::string standard = "123456789";
            CHECK(crc32(reinterpret_cast<const uint8_t*>(standard.data()), standard.size()) == 0xcbf43926U);
        } else if (test == "split_multiple") {
            for (size_t cut = 1; cut < bytes.size(); ++cut) {
                Decoder part; CHECK(part.feed(bytes.data(), cut).empty());
                CHECK(part.feed(bytes.data() + cut, bytes.size() - cut).size() == 1);
            }
            auto copy = bytes; bytes.insert(bytes.end(), copy.begin(), copy.end());
            CHECK(d.feed(bytes).size() == 2);
        } else if (test == "crc") {
            auto bad = bytes; bad[10] ^= 1;
            CHECK(d.feed(bad).empty()); CHECK(d.crc_errors == 1); CHECK(d.feed(bytes).size() == 1);
        } else if (test == "length") {
            auto bad = bytes; bad[2] = 0xff; bad[3] = 0xff;
            CHECK(d.feed(bad).empty()); CHECK(d.errors > 0); CHECK(d.feed(bytes).size() == 1);
            frame.payload.resize(65); bool threw = false;
            try { encode(frame); } catch (...) { threw = true; } CHECK(threw);
        } else if (test == "version") {
            auto bad = bytes; bad[1] = 2;
            CHECK(d.feed(bad).empty()); CHECK(d.errors > 0); CHECK(d.feed(bytes).size() == 1);
        } else if (test == "state") {
            CHECK(transition(State::RECEIVED, State::STAINED)); CHECK(!transition(State::RECEIVED, State::IMAGED));
            CHECK(!transition(State::DONE, State::FAILED)); CHECK(transition(State::HEATED, State::FAILED));
        } else if (test == "queue") {
            WorkQueue q(2); int sum = 0;
            std::thread consumer([&] { int x; while (q.pop(x)) sum += x; });
            for (int i = 1; i <= 100; ++i) CHECK(q.push(i));
            q.close(); consumer.join(); CHECK(sum == 5050); CHECK(!q.push(1));
        } else throw std::runtime_error("unknown test");
        std::cout << test << " PASS\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
