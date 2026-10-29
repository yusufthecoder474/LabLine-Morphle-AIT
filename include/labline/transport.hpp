#pragma once
#include "labline/core.hpp"
#include <future>
#include <map>
#include <thread>
namespace labline {
void nonblocking(int fd);
bool send_bytes(int fd, const Bytes& data);
struct Reply { bool ok = false; std::string reason; };
class Transport {
    struct Pending {
        Frame frame;
        int job = 0, retries = 0;
        Time first, sent;
        std::promise<Reply> promise;
    };
    int fd_;
    Audit& audit_;
    std::mutex mutex_;
    std::map<uint32_t, Pending> pending_;
    uint32_t next_ = 1;
    bool stopped_ = false;
    std::thread thread_;
    Decoder decoder_;
    void loop();
    void fail_locked(const std::string& reason);
public:
    size_t retries = 0, crc_rejects = 0;
    std::vector<double> response_ms;
    Transport(int fd, Audit& audit);
    ~Transport();
    Reply request(int job, int device, int operation);
    void stop();
};
}
