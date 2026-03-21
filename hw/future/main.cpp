#include "future.h"
#include "process_pool.h"
#include "thread_pool.h"

#include <iostream>

int main() {
    {
        hw::future::ThreadPool tp(4);
        auto f = tp.Submit([] { return 42; });
        std::cout << "ThreadPool result: " << f.Get() << std::endl;
    }

    {
        hw::future::ProcessPool pp(2);
        auto f = pp.Submit([] { return 100; });
        std::cout << "ProcessPool result: " << f.Get() << std::endl;
    }

    return 0;
}
