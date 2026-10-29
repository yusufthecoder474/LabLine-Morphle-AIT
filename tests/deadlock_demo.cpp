#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::string mode = argv[1];
    if (mode != "--demo-deadlock=unsafe" && mode != "--demo-deadlock=safe") return 2;
    std::mutex arm, heater, rendezvous;
    std::condition_variable cv;
    int ready = 0;
    auto task = [&](bool reverse) {
        if (mode == "--demo-deadlock=safe") {
            std::lock_guard<std::mutex> a(arm);
            std::lock_guard<std::mutex> h(heater);
            return;
        }
        std::unique_lock<std::mutex> first(reverse ? heater : arm);
        {
            std::unique_lock<std::mutex> gate(rendezvous);
            ++ready;
            cv.notify_all();
            cv.wait(gate, [&] { return ready == 2; });
            std::cout << "first_lock_held_waiting_for_second\n" << std::flush;
        }
        std::unique_lock<std::mutex> second(reverse ? arm : heater);
    };
    std::thread a(task, false), b(task, true);
    a.join(); b.join();
    std::cout << "safe completed\n";
}
