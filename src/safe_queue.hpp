#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>
namespace VTR
{
template<typename T>
class SafeQueue {
    std::queue<T> q;
    std::mutex m;
    std::condition_variable cv;
public:
    void push(T val) {
        std::lock_guard<std::mutex> lock(m);
        q.push(std::move(val));
        cv.notify_one();
    }

    bool pop(T& val) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [this]{ return !q.empty(); });
        val = std::move(q.front());
        q.pop();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m);
        while (!q.empty()) {
            q.pop();
        }
    }
};
using Que = SafeQueue<std::vector<uint8_t>>;
}
