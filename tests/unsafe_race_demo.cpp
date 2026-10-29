// OPTIONAL intentionally undefined behavior. Never linked into LabLine.
#include <thread>
int main() {
    int counter = 0;
    auto increment = [&] { for (int i = 0; i < 100000; ++i) ++counter; };
    std::thread a(increment), b(increment);
    a.join(); b.join();
    return counter == 0;
}
