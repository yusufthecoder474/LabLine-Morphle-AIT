#include "labline/transport.hpp"
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
namespace labline {
void nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl failed");
}
bool send_bytes(int fd, const Bytes& data) {
    size_t offset = 0;
    auto end = Clock::now() + std::chrono::milliseconds(50);
    while (offset < data.size() && Clock::now() < end) {
        auto n = send(fd, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
        if (n > 0) offset += size_t(n);
        else if (n < 0 && errno == EINTR) continue;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pollfd p{fd, POLLOUT, 0};
            poll(&p, 1, 2);
        } else return false;
    }
    return offset == data.size();
}
Transport::Transport(int fd, Audit& audit) : fd_(fd), audit_(audit) {
    nonblocking(fd_);
    thread_ = std::thread(&Transport::loop, this);
}
Transport::~Transport() { stop(); }
void Transport::fail_locked(const std::string& reason) {
    stopped_ = true;
    shutdown(fd_, SHUT_RDWR);
    for (auto& item : pending_) {
        auto& p = item.second;
        audit_.log(p.job, item.first, "ERROR", p.frame.device, "UNKNOWN", reason);
        p.promise.set_value({false, reason});
    }
    pending_.clear();
}
Reply Transport::request(int job, int device, int operation) {
    std::future<Reply> future;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return {false, "transport_closed_not_issued"};
        if (pending_.size() >= 16) return {false, "pending_capacity_not_issued"};
        Pending p;
        p.job = job;
        p.frame = {next_++, uint8_t(device), uint8_t(operation), {}};
        put32(p.frame.payload, uint32_t(job));
        future = p.promise.get_future();
        audit_.log(job, p.frame.sequence, "REQUEST", device, std::to_string(operation));
        pending_.emplace(p.frame.sequence, std::move(p));
    }
    // The loop resolves every promise on ACK, timeout, disconnect, or shutdown.
    return future.get();
}
void Transport::loop() {
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopped_) break;
            for (auto& item : pending_) {
                auto& p = item.second;
                auto now = Clock::now();
                bool first = p.sent == Time{};
                if (!first && ms(p.sent, now) < 80) continue;
                if (!first && p.retries == 2) {
                    fail_locked("timeout_execution_unknown_run_aborted");
                    break;
                }
                if (!first) {
                    ++p.retries;
                    ++retries;
                    audit_.log(p.job, item.first, "RETRY", p.frame.device, "SAME_SEQUENCE");
                } else p.first = now;
                p.sent = now;
                if (!send_bytes(fd_, encode(p.frame))) {
                    fail_locked("send_disconnect_execution_unknown");
                    break;
                }
            }
            if (stopped_) break;
        }
        pollfd poller{fd_, POLLIN, 0};
        int result = poll(&poller, 1, 1);
        if (result < 0 && errno == EINTR) continue;
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) break;
        if (result < 0 || (poller.revents & (POLLERR | POLLNVAL))) {
            fail_locked("poll_disconnect_execution_unknown");
            break;
        }
        if (poller.revents & (POLLIN | POLLHUP)) {
            uint8_t bytes[2048];
            auto n = recv(fd_, bytes, sizeof bytes, 0);
            if (n == 0) { fail_locked("peer_disconnect_execution_unknown"); break; }
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                fail_locked("read_disconnect_execution_unknown");
                break;
            }
            auto old = decoder_.crc_errors;
            auto frames = decoder_.feed(bytes, size_t(n));
            crc_rejects += decoder_.crc_errors - old;
            if (decoder_.crc_errors > old) audit_.log(0, 0, "CRC_REJECT", 0, "NO_STATE_CHANGE");
            for (const auto& frame : frames) {
                auto it = pending_.find(frame.sequence);
                if (it == pending_.end()) continue;
                auto& p = it->second;
                if (frame.device != p.frame.device || frame.payload.size() != 8 ||
                    get32(frame.payload, 0) != uint32_t(p.job) ||
                    (frame.operation != 0x80 && frame.operation != 0x81)) continue;
                bool ok = frame.operation == 0x80;
                response_ms.push_back(ms(p.first, Clock::now()));
                audit_.log(p.job, frame.sequence, ok ? "ACK" : "NACK", frame.device,
                           ok ? "CONFIRMED" : "REJECTED", "result_" + std::to_string(get32(frame.payload, 4)));
                p.promise.set_value({ok, ok ? "" : "device_rejected_not_executed"});
                pending_.erase(it);
            }
        }
    }
}
void Transport::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stopped_) fail_locked("controller_shutdown");
    }
    if (thread_.joinable()) thread_.join();
}
}
