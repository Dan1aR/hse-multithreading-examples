#include "apply_function.h"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <vector>

TEST(ApplyFunction, AppliesTransformSingleThread) {
    std::vector<int> data{1, 2, 3, 4, 5};

    ApplyFunction<int>(data, [](int& value) { value *= 2; }, 1);

    EXPECT_EQ(data, (std::vector<int>{2, 4, 6, 8, 10}));
}

TEST(ApplyFunction, AppliesTransformMultiThread) {
    std::vector<int> data(1000, 1);

    ApplyFunction<int>(data, [](int& value) { ++value; }, 8);

    for (const int value : data) {
        EXPECT_EQ(value, 2);
    }
}

TEST(ApplyFunction, ClampsThreadCountToDataSize) {
    std::vector<int> data{1, 2, 3};

    ApplyFunction<int>(data, [](int& value) { value += 10; }, 100);

    EXPECT_EQ(data, (std::vector<int>{11, 12, 13}));
}

TEST(ApplyFunction, NonPositiveThreadCountBehavesAsSingleThread) {
    std::vector<int> data{2, 4, 6};

    ApplyFunction<int>(data, [](int& value) { value /= 2; }, 0);
    EXPECT_EQ(data, (std::vector<int>{1, 2, 3}));

    ApplyFunction<int>(data, [](int& value) { value *= 3; }, -5);
    EXPECT_EQ(data, (std::vector<int>{3, 6, 9}));
}

TEST(ApplyFunction, WorksWithAnotherType) {
    std::vector<std::string> data{"a", "bb", "ccc"};

    ApplyFunction<std::string>(data, [](std::string& value) { value += "!"; }, 3);

    EXPECT_EQ(data, (std::vector<std::string>{"a!", "bb!", "ccc!"}));
}

TEST(ApplyFunction, DoesNothingForEmptyVector) {
    std::vector<int> data;
    std::atomic<int> calls{0};

    ApplyFunction<int>(data, [&calls](int&) { ++calls; }, 4);

    EXPECT_EQ(calls.load(), 0);
}
