#include "labline/core.hpp"
#include <iostream>
#include <regex>
int main(int argc, char** argv) {
    try {
        labline::Config c;
        for (int i = 1; i < argc; ++i) {
            std::string key = argv[i];
            if (key == "--help") {
                std::cout << "LabLine: software-only lab station simulation\n--jobs 1..100 --workers 1|4 --seed 0..100000 --deadline-ms N --fault none|ack|crc|slow|crash|timeout --out DIRECTORY --run-id NAME\n";
                return 0;
            }
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            std::string value = argv[++i];
            if (key == "--jobs") c.jobs = std::stoi(value);
            else if (key == "--workers") c.workers = std::stoi(value);
            else if (key == "--seed") c.seed = std::stoi(value);
            else if (key == "--deadline-ms") c.deadline_ms = std::stoi(value);
            else if (key == "--fault") c.fault = value;
            else if (key == "--out") c.output = value;
            else if (key == "--run-id") c.run_id = value;
            else throw std::runtime_error("unknown option");
        }
        if (c.jobs < 1 || c.jobs > 100 || (c.workers != 1 && c.workers != 4) || c.seed < 0 || c.seed > 100000 || c.deadline_ms < 1 ||
            !std::regex_match(c.run_id, std::regex("[A-Za-z0-9_-]{1,64}")) ||
            !std::regex_match(c.fault, std::regex("none|ack|crc|slow|crash|timeout"))) throw std::runtime_error("invalid configuration");
        return labline::run(c);
    } catch (const std::exception& e) {
        std::cerr << "LabLine: " << e.what() << '\n';
        return 1;
    }
}
