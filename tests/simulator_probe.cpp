#include "labline/core.hpp"
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    labline::Config c; c.output = argv[2];
    return labline::simulator(std::stoi(argv[1]), c);
}
