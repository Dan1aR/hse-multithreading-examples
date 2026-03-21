#include "condition_variable.h"

#include <iostream>

int main() {
    [[maybe_unused]] hw::futex::FutexConditionVariable conditionVariable;
    std::cout << "hw/futex scaffold is ready" << std::endl;
    return 0;
}
