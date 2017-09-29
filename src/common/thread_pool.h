// Copyright 2016 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "common/assert.h"

namespace Common {

class ThreadPool {
private:
    explicit ThreadPool(unsigned int num_threads) : num_threads(num_threads), workers(num_threads) {
        ASSERT(num_threads);
    }

public:
    static ThreadPool& GetPool() {
        static ThreadPool thread_pool(std::thread::hardware_concurrency());
        return thread_pool;
    }

    template <typename F, typename... Args>
    auto push(F&& f, Args&&... args) {
        auto ret = workers[next_worker].push(std::forward<F>(f), std::forward<Args>(args)...);
        next_worker = (next_worker + 1) % num_threads;
        return ret;
    }

    unsigned int total_threads() {
        return num_threads;
    }

private:
    template <typename T>
    class ThreadsafeQueue {
    private:
        const size_t capacity;
        std::vector<T> queue_storage;
        std::mutex mutex;
        std::condition_variable queue_changed;

    public:
        explicit ThreadsafeQueue(const size_t capacity) : capacity(capacity) {
            queue_storage.reserve(capacity);
        }

        void push(const T& element) {
            std::unique_lock<std::mutex> lock(mutex);
            while (queue_storage.size() >= capacity) {
                queue_changed.wait(lock);
            }
            queue_storage.push_back(element);
            queue_changed.notify_one();
        }

        T pop() {
            std::unique_lock<std::mutex> lock(mutex);
            while (queue_storage.empty()) {
                queue_changed.wait(lock);
            }
            T element(std::move(queue_storage.back()));
            queue_storage.pop_back();
            queue_changed.notify_one();
            return element;
        }

        void push(T&& element) {
            std::unique_lock<std::mutex> lock(mutex);
            while (queue_storage.size() >= capacity) {
                queue_changed.wait(lock);
            }
            queue_storage.emplace_back(element);
            queue_changed.notify_one();
        }
    };

    class Worker {
    private:
        ThreadsafeQueue<std::function<void()>> queue;
        std::thread thread;
        static constexpr size_t MAX_QUEUE_CAPACITY = 100;

    public:
        Worker() : queue(MAX_QUEUE_CAPACITY), thread([this] { loop(); }) {}

        ~Worker() {
            queue.push(nullptr); // Exit the loop
            thread.join();
        }

        void loop() {
            for (;;) {
                std::function<void()> fn(queue.pop());
                if (!fn) // a nullptr function is the signal to exit the loop
                    break;
                fn();
            }
        }

        template <typename F, typename... Args>
        auto push(F&& f, Args&&... args) {
            auto task = std::make_shared<std::packaged_task<decltype(f(args...))()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            queue.push([task]() { (*task)(); });
            return task->get_future();
        }
    };

    const unsigned int num_threads;
    int next_worker = 0;
    std::vector<Worker> workers;
};

} // namespace Common
