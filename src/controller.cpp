#include "labline/transport.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
namespace labline {
namespace {
class Child {
    pid_t pid_ = -1;
public:
    int fd = -1;
    explicit Child(const Config& c) {
        int sockets[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets)) throw std::runtime_error("socketpair failed");
        pid_ = fork(); // Main has not started any threads yet.
        if (pid_ == 0) {
            close(sockets[0]);
            int result = 2;
            try { result = simulator(sockets[1], c); } catch (...) { result = 3; }
            close(sockets[1]);
            _exit(result);
        }
        close(sockets[1]);
        if (pid_ < 0) { close(sockets[0]); throw std::runtime_error("fork failed"); }
        fd = sockets[0];
    }
    ~Child() {
        if (fd >= 0) { shutdown(fd, SHUT_RDWR); close(fd); }
        for (int i = 0; i < 100; ++i) {
            if (waitpid(pid_, nullptr, WNOHANG) == pid_) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        kill(pid_, SIGKILL);
        waitpid(pid_, nullptr, 0);
    }
};
bool lock_until(std::unique_lock<std::mutex>& lock, Time until) {
    while (Clock::now() < until) {
        if (lock.try_lock()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
struct Job {
    State state = State::RECEIVED;
    Time accepted;
    double queue_ms = 0, total_ms = 0;
    bool unknown = false;
};
double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[std::max<size_t>(1, size_t(std::ceil(p * v.size()))) - 1];
}
}
int run(const Config& c) {
    std::filesystem::create_directories(c.output);
    Child child(c);
    Audit audit(c.output + "/events.csv", c.run_id);
    Transport transport(child.fd, audit);
    std::array<std::mutex, 6> devices;
    WorkQueue queue(8);
    std::vector<Job> jobs(size_t(c.jobs + 1));
    std::vector<std::thread> workers;
    auto begin = Clock::now();
    auto worker = [&] {
        int id;
        while (queue.pop(id)) {
            auto& job = jobs[size_t(id)]; // Sole worker owns this job after dequeue.
            job.queue_ms = ms(job.accepted, Clock::now());
            auto advance = [&](State next, const std::string& reason = "") {
                if (!transition(job.state, next)) throw std::runtime_error("illegal state transition");
                job.state = next;
                audit.log(id, 0, "STATE", 0, name(next), reason);
            };
            for (int step = 1; step <= 4; ++step) {
                int device = step == 1 ? 2 : step == 2 ? 4 : step == 3 ? 1 : 5;
                std::unique_lock<std::mutex> first, second;
                auto until = Clock::now() + std::chrono::seconds(5);
                bool locked = false;
                if (step == 1) {
                    // Equivalent pumps: never hold one while waiting for the other.
                    while (Clock::now() < until) {
                        for (int pump = 2; pump <= 3; ++pump) {
                            std::unique_lock<std::mutex> candidate(devices[pump], std::try_to_lock);
                            if (candidate.owns_lock()) { first = std::move(candidate); device = pump; locked = true; break; }
                        }
                        if (locked) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } else {
                    first = std::unique_lock<std::mutex>(devices[device], std::defer_lock);
                    locked = lock_until(first, until);
                    if (locked && step == 3) {
                        // Arm (1) before heater (4), always in ascending order.
                        second = std::unique_lock<std::mutex>(devices[4], std::defer_lock);
                        locked = lock_until(second, until);
                    }
                }
                if (!locked) { advance(State::FAILED, "resource_timeout_not_issued"); break; }
                Reply reply = transport.request(id, device, step);
                if (!reply.ok) {
                    job.unknown = reply.reason.find("unknown") != std::string::npos;
                    advance(State::FAILED, reply.reason);
                    break;
                }
                advance(static_cast<State>(step));
            }
            if (job.state == State::IMAGED) advance(State::DONE);
            job.total_ms = ms(job.accepted, Clock::now());
            audit.log(id, 0, "TERMINAL", 0, name(job.state), job.unknown ? "UNKNOWN" : "ACCOUNTED");
        }
    };
    for (int i = 0; i < c.workers; ++i) workers.emplace_back(worker);
    for (int id = 1; id <= c.jobs; ++id) {
        jobs[size_t(id)].accepted = Clock::now();
        audit.log(id, 0, "ACCEPT", 0, "RECEIVED", "LAB-" + std::to_string(id));
        queue.push(id);
    }
    queue.close();
    for (auto& thread : workers) thread.join();
    transport.stop();
    double duration = ms(begin, Clock::now());
    std::vector<double> times, waits;
    int done = 0, failed = 0, unknown = 0, misses = 0;
    std::ofstream detail(c.output + "/jobs.csv");
    detail << "job_id,state,unknown,queue_ms,total_ms\n";
    for (int id = 1; id <= c.jobs; ++id) {
        const auto& j = jobs[size_t(id)];
        done += j.state == State::DONE;
        failed += j.state == State::FAILED;
        unknown += j.unknown;
        misses += j.total_ms > c.deadline_ms;
        times.push_back(j.total_ms);
        waits.push_back(j.queue_ms);
        detail << id << ',' << name(j.state) << ',' << j.unknown << ',' << j.queue_ms << ',' << j.total_ms << '\n';
    }
    double mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    double ipc_mean = transport.response_ms.empty() ? 0 : std::accumulate(transport.response_ms.begin(), transport.response_ms.end(), 0.0) / transport.response_ms.size();
    double variance = 0;
    for (double value : transport.response_ms) variance += (value - ipc_mean) * (value - ipc_mean);
    double jitter = transport.response_ms.empty() ? 0 : std::sqrt(variance / transport.response_ms.size());
    std::ofstream summary(c.output + "/summary.csv");
    summary << "run_id,jobs,workers,seed,fault,done,failed,unknown,duration_ms,throughput,mean_ms,median_ms,p95_ms,p99_ms,p95_ipc_ms,jitter_ms,p95_queue_ms,deadline_ms,deadline_misses,retries,crc_rejects\n";
    summary << std::fixed << std::setprecision(4) << c.run_id << ',' << c.jobs << ',' << c.workers << ',' << c.seed << ',' << c.fault << ',' << done << ',' << failed << ',' << unknown << ',' << duration << ',' << c.jobs * 1000 / duration << ',' << mean << ',' << percentile(times, .5) << ',' << percentile(times, .95) << ',' << percentile(times, .99) << ',' << percentile(transport.response_ms, .95) << ',' << jitter << ',' << percentile(waits, .95) << ',' << c.deadline_ms << ',' << misses << ',' << transport.retries << ',' << transport.crc_rejects << '\n';
    std::cout << c.run_id << ": accepted=" << c.jobs << " DONE=" << done << " FAILED=" << failed
              << " UNKNOWN(subset of FAILED)=" << unknown << " retries=" << transport.retries
              << " CRC rejects=" << transport.crc_rejects << "\n";
    return failed ? 2 : 0;
}
}
