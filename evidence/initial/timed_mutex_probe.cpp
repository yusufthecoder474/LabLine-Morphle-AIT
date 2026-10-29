#include <mutex>
#include <chrono>
int main() {
    std::timed_mutex mutex;
    if (mutex.try_lock_until(std::chrono::steady_clock::now() + std::chrono::seconds(1))) mutex.unlock();
}
