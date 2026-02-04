#pragma once
#include <iostream>
#include <chrono>
#include <string>
#include <format>

template <bool Enable>
class TimeAnalyzerImpl {
public:
    TimeAnalyzerImpl(const std::string&) {}
    ~TimeAnalyzerImpl() = default;
};

template <>
class TimeAnalyzerImpl<true> {
    const std::string name;
    const std::chrono::high_resolution_clock::time_point start;

public:
    TimeAnalyzerImpl(const std::string& n) 
        : name(n), start(std::chrono::high_resolution_clock::now()) {}

    ~TimeAnalyzerImpl() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << std::format("[Timer] {} : {} us", name, duration) << std::endl;
    }
};

#ifdef DEBUG
    using TimeAnalyzer = TimeAnalyzerImpl<true>;
#else
    using TimeAnalyzer = TimeAnalyzerImpl<false>;
#endif