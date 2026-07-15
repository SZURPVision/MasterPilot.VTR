#pragma once
#include <chrono>
#include <string>
#include <godot_cpp/variant/utility_functions.hpp>

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
		godot::UtilityFunctions::print("[Timer] ", name.c_str()," : ",duration);
    }
};

#ifdef DEBUG
    using TimeAnalyzer = TimeAnalyzerImpl<true>;
#else
    using TimeAnalyzer = TimeAnalyzerImpl<false>;
#endif