#include "labline/transport.hpp"
#include <array>
#include <map>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
namespace labline {
int simulator(int fd, const Config& config) {
    nonblocking(fd);
    std::ofstream log(config.output + "/simulator.csv");
    if (!log) return 2;
    log << "sequence,job_id,device,operation,event,steady_elapsed_ns\n";
    auto origin = Clock::now();
    auto record = [&](const Frame& f, const std::string& event) {
        log << f.sequence << ',' << get32(f.payload, 0) << ',' << int(f.device) << ','
            << int(f.operation) << ',' << event << ','
            << std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - origin).count() << '\n';
        log.flush();
    };
    struct Action { Frame request, reply; Time due; bool done = false; };
    std::map<uint32_t, Action> actions;
    std::array<bool, 6> busy{};
    Decoder decoder;
    bool injected = false;
    auto reply_for = [](const Frame& f, bool ok) {
        Frame r{f.sequence, f.device, uint8_t(ok ? 0x80 : 0x81), f.payload};
        put32(r.payload, ok ? f.sequence : 0);
        return r;
    };
    for (;;) {
        auto now = Clock::now();
        for (auto& entry : actions) {
            auto& a = entry.second;
            if (a.done || a.due > now) continue;
            a.done = true;
            busy[a.request.device] = false;
            record(a.request, "COMPLETE");
            bool target = get32(a.request.payload, 0) == 1 && a.request.operation == 1;
            if (target && !injected && (config.fault == "ack" || config.fault == "timeout")) {
                injected = true;
                record(a.request, "ACK_DROPPED");
                continue;
            }
            auto bytes = encode(a.reply);
            if (target && !injected && config.fault == "crc") {
                injected = true;
                bytes[10] ^= 1;
                record(a.request, "REPLY_CORRUPTED");
            }
            if (!send_bytes(fd, bytes)) return 0;
        }
        pollfd p{fd, POLLIN, 0};
        if (poll(&p, 1, 1) < 0) continue;
        if (p.revents & (POLLERR | POLLNVAL)) return 0;
        if (!(p.revents & (POLLIN | POLLHUP))) continue;
        uint8_t bytes[2048];
        auto n = recv(fd, bytes, sizeof bytes, 0);
        if (n == 0) return 0;
        if (n < 0) continue;
        auto frames = decoder.feed(bytes, size_t(n));
        for (const auto& f : frames) {
            if (f.payload.size() != 4) continue;
            auto it = actions.find(f.sequence);
            if (it != actions.end()) {
                auto& a = it->second;
                if (a.request.device != f.device || a.request.operation != f.operation || a.request.payload != f.payload) {
                    if (!send_bytes(fd, encode(reply_for(f, false)))) return 0;
                    continue;
                }
                record(f, "DEDUP");
                if (config.fault == "timeout" && get32(f.payload, 0) == 1 && f.operation == 1) continue;
                if (a.done && !send_bytes(fd, encode(a.reply))) return 0;
                continue;
            }
            bool valid = ((f.device == 2 || f.device == 3) && f.operation == 1) ||
                         (f.device == 4 && f.operation == 2) ||
                         (f.device == 1 && f.operation == 3) || (f.device == 5 && f.operation == 4);
            if (!valid || busy[f.device] || actions.size() >= 512) {
                if (!send_bytes(fd, encode(reply_for(f, false)))) return 0;
                continue;
            }
            if (config.fault == "crash" && get32(f.payload, 0) == 1 && f.operation == 1) {
                record(f, "INJECTED_CRASH");
                return 9;
            }
            // Stateless deterministic delay: identical logical job/operation inputs in either worker mode.
            int delay = 3 + (config.seed + int(get32(f.payload, 0)) * 7 + f.operation * 11) % 5;
            if (config.fault == "slow" && f.device == 4) delay += 45;
            busy[f.device] = true;
            actions.emplace(f.sequence, Action{f, reply_for(f, true), Clock::now() + std::chrono::milliseconds(delay), false});
            record(f, "START");
        }
    }
}
}
