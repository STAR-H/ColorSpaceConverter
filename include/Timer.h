#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <iostream>

class Timer {
public:
    Timer() : m_start(std::chrono::high_resolution_clock::now()) {}
    ~Timer() {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> duration = end - m_start;
        std::cout << "耗时 " << duration.count() << " 秒" << std::endl;
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

#endif // TIMER_H
