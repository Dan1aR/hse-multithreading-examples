#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

template <typename T>
void ApplyFunction(std::vector<T>& data, const std::function<void(T&)>& transform,
                   const int threadCount = 1) {
    if (data.empty()) {
        return;
    }

    const std::size_t safeThreadCount = std::min<std::size_t>(
        data.size(), static_cast<std::size_t>(std::max(1, threadCount)));

    if (safeThreadCount == 1) {
        for (auto& element : data) {
            transform(element);
        }
        return;
    }

    std::vector<std::thread> threads;
    threads.reserve(safeThreadCount);

    const std::size_t baseChunkSize = data.size() / safeThreadCount;
    const std::size_t remainder = data.size() % safeThreadCount;

    std::size_t begin = 0;
    for (std::size_t i = 0; i < safeThreadCount; ++i) {
        const std::size_t chunkSize = baseChunkSize + (i < remainder ? 1 : 0);
        const std::size_t end = begin + chunkSize;

        threads.emplace_back([&, begin, end]() {
            for (std::size_t index = begin; index < end; ++index) {
                transform(data[index]);
            }
        });

        begin = end;
    }

    for (auto& thread : threads) {
        thread.join();
    }
}
