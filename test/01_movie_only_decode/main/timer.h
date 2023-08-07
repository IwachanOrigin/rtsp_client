
#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>

class Timer
{
public:
    explicit Timer() { restart(); }
    ~Timer() {}

    std::wstring now()
    {
        struct tm timeinfo;
        errno_t err = 0;

        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        err = localtime_s(&timeinfo, &now_c);
        if (err != 0) {
            std::wcerr << "Failed create log file." << std::endl;
            return L"";
        }

        std::wstringstream ss;
        ss << std::put_time(&timeinfo, L"%Y/%m/%d_%H:%M:%S");
        return ss.str();
    }

    long long elapsed()
    {
        auto end = std::chrono::system_clock::now();
        auto dur = end - m_start;
        return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
    }

    long long elapsed_micro()
    {
        auto end = std::chrono::system_clock::now();
        auto dur = end - m_start;
        return std::chrono::duration_cast<std::chrono::microseconds>(dur).count();
    }

private:
    void restart()
    {
        m_start = std::chrono::system_clock::now();
    }

    std::chrono::system_clock::time_point m_start;
};

#endif // TIMER_H