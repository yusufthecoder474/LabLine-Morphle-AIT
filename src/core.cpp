#include "labline/core.hpp"
namespace labline {
std::string name(State s) {
    static const char* names[] = {"RECEIVED", "STAINED", "HEATED", "PLACED", "IMAGED", "DONE", "FAILED"};
    return names[static_cast<int>(s)];
}
bool transition(State from, State to) {
    if (from == State::DONE || from == State::FAILED) return false;
    return to == State::FAILED || static_cast<int>(to) == static_cast<int>(from) + 1;
}
Audit::Audit(const std::string& path, const std::string& run) : file_(path), run_(run) {
    if (!file_) throw std::runtime_error("cannot open audit log");
    file_ << "run_id,job_id,sequence,event,device,steady_elapsed_ns,result,reason\n";
    file_.exceptions(std::ios::badbit | std::ios::failbit);
}
void Audit::log(int job, uint32_t seq, const std::string& event, int device,
                const std::string& result, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - origin_).count();
    file_ << run_ << ',' << job << ',' << seq << ',' << event << ',' << device << ','
          << ns << ',' << result << ',' << reason << '\n';
    file_.flush();
}
bool WorkQueue::push(int job) {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait(lock, [&] { return closed_ || jobs_.size() < capacity_; });
    if (closed_) return false;
    jobs_.push_back(job);
    changed_.notify_all();
    return true;
}
bool WorkQueue::pop(int& job) {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait(lock, [&] { return closed_ || !jobs_.empty(); });
    if (jobs_.empty()) return false;
    job = jobs_.front();
    jobs_.pop_front();
    changed_.notify_all();
    return true;
}
void WorkQueue::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    changed_.notify_all();
}
}
