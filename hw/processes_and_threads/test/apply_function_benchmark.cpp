#include "apply_function.h"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

namespace {

std::vector<int> MakeRandomSleepsUs(const std::size_t size, const int minSleepUs,
                                    const int maxSleepUs) {
    std::mt19937 generator(42);
    std::uniform_int_distribution<int> distribution(minSleepUs, maxSleepUs);

    std::vector<int> sleepsUs(size);
    for (int& value : sleepsUs) {
        value = distribution(generator);
    }
    return sleepsUs;
}

int GetMultiThreadCount() {
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    return std::max(2, static_cast<int>(hardwareThreads == 0 ? 4 : hardwareThreads));
}

}  // namespace

void CpuBoundSingleThread(benchmark::State& state) {
    constexpr std::size_t kDataSize = 64;
    std::vector<int> data(kDataSize, 3);

    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 3);
        ApplyFunction<int>(data, [](int& value) { value = value * value; }, 1);
        benchmark::DoNotOptimize(data);
    }
}

void CpuBoundMultiThread(benchmark::State& state) {
    constexpr std::size_t kDataSize = 64;
    std::vector<int> data(kDataSize, 3);
    const int threadCount = state.range(0);

    for (auto _ : state) {
        std::fill(data.begin(), data.end(), 3);
        ApplyFunction<int>(data, [](int& value) { value = value * value; }, threadCount);
        benchmark::DoNotOptimize(data);
    }
}

void IoBoundSingleThread(benchmark::State& state) {
    constexpr std::size_t kDataSize = 24;
    auto data = MakeRandomSleepsUs(kDataSize, 200, 600);

    for (auto _ : state) {
        ApplyFunction<int>(
            data,
            [](int& sleepUs) { std::this_thread::sleep_for(std::chrono::microseconds(sleepUs)); },
            1);
        benchmark::DoNotOptimize(data);
    }
}

void IoBoundMultiThread(benchmark::State& state) {
    constexpr std::size_t kDataSize = 24;
    auto data = MakeRandomSleepsUs(kDataSize, 200, 600);
    const int threadCount = state.range(0);

    for (auto _ : state) {
        ApplyFunction<int>(
            data,
            [](int& sleepUs) { std::this_thread::sleep_for(std::chrono::microseconds(sleepUs)); },
            threadCount);
        benchmark::DoNotOptimize(data);
    }
}

BENCHMARK(CpuBoundSingleThread)->Unit(benchmark::kMicrosecond)->MinTime(0.2);
BENCHMARK(CpuBoundMultiThread)
    ->Arg(GetMultiThreadCount())
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(0.2);

BENCHMARK(IoBoundSingleThread)->UseRealTime()->Unit(benchmark::kMillisecond)->MinTime(0.2);
BENCHMARK(IoBoundMultiThread)
    ->Arg(GetMultiThreadCount())
    ->UseRealTime()
    ->Unit(benchmark::kMillisecond)
    ->MinTime(0.2);

BENCHMARK_MAIN();
