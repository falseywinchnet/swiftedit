#include "terminal_page.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
double process_cpu_ms() {
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        throw std::runtime_error("Cannot read benchmark process CPU time.");
    const std::uint64_t kernel_ticks = (static_cast<std::uint64_t>(kernel.dwHighDateTime) << 32) |
                                       kernel.dwLowDateTime;
    const std::uint64_t user_ticks = (static_cast<std::uint64_t>(user.dwHighDateTime) << 32) |
                                     user.dwLowDateTime;
    const double result = static_cast<double>(kernel_ticks) / 10000.0 +
                          static_cast<double>(user_ticks) / 10000.0;
#else
    const std::clock_t ticks = std::clock();
    if (ticks == std::clock_t(-1) || ticks < 0)
        throw std::runtime_error("Benchmark process CPU clock is unavailable.");
    const double result = 1000.0 * static_cast<double>(ticks) / CLOCKS_PER_SEC;
#endif
    return result;
}
double milliseconds(const Clock::time_point start) {
    const std::chrono::duration<double, std::milli> elapsed = Clock::now() - start;
    const double result = elapsed.count();
    return result;
}
void report(const std::string_view fixture, const std::string_view operation,
            std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const std::size_t p50 = (samples.size() * 50 + 99) / 100 - 1;
    const std::size_t p95 = (samples.size() * 95 + 99) / 100 - 1;
    const std::size_t p99 = (samples.size() * 99 + 99) / 100 - 1;
    std::cout << fixture << ',' << operation << ",samples=" << samples.size()
              << ",p50=" << samples[p50] << ",p95=" << samples[p95]
              << ",p99=" << samples[p99] << ",worst=" << samples.back() << '\n';
}
void measure(const std::filesystem::path &directory, const std::string &name,
             const std::string &pattern, std::ofstream &raw) {
    const std::filesystem::path path = directory / (name + ".txt");
    {
        std::ofstream file(path, std::ios::binary);
        std::string block{};
        while (block.size() < 32768)
            block += pattern;
        std::size_t remaining = swiftedit::editable_limit - 1;
        while (remaining) {
            const std::size_t count = std::min(remaining, block.size());
            file.write(block.data(), static_cast<std::streamsize>(count));
            remaining -= count;
        }
        file.put('Z');
        if (!file)
            throw std::runtime_error("Cannot write owned benchmark fixture.");
    }
    swiftedit::Session session{};
    session.open(path);
    if (!session.read_only() || session.size() != swiftedit::editable_limit)
        throw std::runtime_error("Benchmark fixture did not enter paged read-only mode.");
    std::vector<double> slices{}, cpu_slices{}, totals{}, cpu_totals{}, publications{}, releases{}, cancellations{};
    slices.reserve(16384);
    cpu_slices.reserve(16384);
    for (std::size_t trial = 0; trial < 5; ++trial) {
        swiftedit::TerminalPager pager{};
        pager.reset(session);
        std::unique_ptr<swiftedit::TerminalPageEnd> task =
            std::make_unique<swiftedit::TerminalPageEnd>(session, 80, 24);
        const Clock::time_point total_start = Clock::now();
        const double total_cpu_start = process_cpu_ms();
        std::vector<double> steps{};
        std::vector<double> cpu_steps{};
        steps.reserve(4096);
        cpu_steps.reserve(4096);
        bool complete = false;
        while (!complete) {
            const Clock::time_point started = Clock::now();
            const double cpu_start = process_cpu_ms();
            complete = (*task).step(session);
            const double cpu_end = process_cpu_ms();
            steps.push_back(milliseconds(started));
            if (cpu_end < cpu_start)
                throw std::runtime_error("Benchmark process CPU clock is unavailable or invalid.");
            const double cpu_ms = cpu_end - cpu_start;
            cpu_steps.push_back(cpu_ms);
            if (steps.size() > 16384)
                throw std::runtime_error("End scan exceeded the fixture work bound.");
        }
        const Clock::time_point publish_start = Clock::now();
        pager.finish_end(session, *task);
        const swiftedit::TerminalPageFrame &frame = pager.frame(session, 80, 24);
        const double publication = milliseconds(publish_start);
        if (frame.more || frame.next.offset != session.size() || frame.runs.empty() ||
            frame.runs.back().text != "Z" || session.dirty())
            throw std::runtime_error("Benchmark failed to present the unchanged final byte.");
        const Clock::time_point release_start = Clock::now();
        task.reset();
        const double release = milliseconds(release_start);
        const double total = milliseconds(total_start);
        const double total_cpu = process_cpu_ms() - total_cpu_start;
        if (total_cpu < 0)
            throw std::runtime_error("Benchmark process CPU clock moved backwards.");
        totals.push_back(total);
        cpu_totals.push_back(total_cpu);
        publications.push_back(publication);
        releases.push_back(release);
        for (std::size_t index = 0; index < steps.size(); ++index) {
            slices.push_back(steps[index]);
            cpu_slices.push_back(cpu_steps[index]);
            raw << name << ',' << trial << ",step," << index << ',' << steps[index] << '\n';
            raw << name << ',' << trial << ",step-cpu," << index << ',' << cpu_steps[index] << '\n';
        }
        raw << name << ',' << trial << ",publish-frame,0," << publication << '\n';
        raw << name << ',' << trial << ",release,0," << release << '\n';
        raw << name << ',' << trial << ",total,0," << total << '\n';
        raw << name << ',' << trial << ",total-cpu,0," << total_cpu << '\n';
        pager.first();
        task = std::make_unique<swiftedit::TerminalPageEnd>(session, 80, 24);
        for (std::size_t index = 0; index < 32; ++index)
            if ((*task).step(session))
                throw std::runtime_error("Cancellation fixture completed too soon.");
        const Clock::time_point cancel_start = Clock::now();
        task.reset();
        const double cancelled = milliseconds(cancel_start);
        if (pager.source_offset() != 0 || session.dirty())
            throw std::runtime_error("Cancellation changed the visible page or document.");
        cancellations.push_back(cancelled);
        raw << name << ',' << trial << ",cancel-release,0," << cancelled << '\n';
    }
    report(name, "step", slices);
    report(name, "step-cpu", cpu_slices);
    report(name, "publish-frame", publications);
    report(name, "release", releases);
    report(name, "cancel-release", cancellations);
    report(name, "total", totals);
    report(name, "total-cpu", cpu_totals);
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Usage: swiftedit-terminal-end-bench new-output-directory");
        const std::filesystem::path directory(argv[1]);
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Benchmark output directory must be new.");
        std::ofstream raw(directory / "samples.csv");
        if (!raw)
            throw std::runtime_error("Cannot create raw benchmark samples.");
        raw << std::fixed << std::setprecision(6);
        raw << "fixture,trial,operation,sample,elapsed_ms\n";
        std::cout << std::fixed << std::setprecision(6);
        measure(directory, "ascii-16m", "x", raw);
        measure(directory, "controls-16m", std::string(1, '\0'), raw);
        measure(directory, "mixed-16m", "a\t\xe7\x95\x8c" "e\xcc\x81\r\n", raw);
        if (!raw)
            throw std::runtime_error("Cannot write raw benchmark samples.");
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
