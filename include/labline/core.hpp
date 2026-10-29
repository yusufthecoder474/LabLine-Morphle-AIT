#pragma once
#include "labline/protocol.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
namespace labline {
using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;
inline double ms(Time begin, Time end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}
enum class State { RECEIVED, STAINED, HEATED, PLACED, IMAGED, DONE, FAILED };
std::string name(State s);
bool transition(State from, State to);
struct Config {
    int jobs = 10, workers = 4, seed = 42, deadline_ms = 200;
    std::string fault = "none", output = "audit", run_id = "demo";
};
class Audit {
    std::mutex mutex_;
    std::ofstream file_;
    Time origin_ = Clock::now();
    std::string run_;
public:
    Audit(const std::string& path, const std::string& run);
    void log(int job, uint32_t seq, const std::string& event, int device,
             const std::string& result, const std::string& reason = "");
};
// Producer blocks on capacity; close wakes both producers and consumers.
class WorkQueue {
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<int> jobs_;
    bool closed_ = false;
    size_t capacity_;
public:
    explicit WorkQueue(size_t capacity) : capacity_(capacity) {}
    bool push(int job);
    bool pop(int& job);
    void close();
};
int simulator(int fd, const Config& config);
int run(const Config& config);
}
