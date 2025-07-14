#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
namespace DMZ {

class ThreadPool {
   public:
#ifdef DMZ_SINGLE_THREADED
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {}

    void submit(std::function<void()> task) { task(); }

    void wait() {}

    ~ThreadPool() {}

#else
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) : stop(false), active_tasks(0) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

                        if (this->stop && this->tasks.empty()) {
                            return;
                        }
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();

                    active_tasks--;
                    completion_condition.notify_one();
                }
            });
        }
    }

    void submit(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(std::move(task));
            active_tasks++;
        }
        condition.notify_one();
    }

    void wait() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        completion_condition.wait(lock, [this] { return tasks.empty() && active_tasks == 0; });
    }

    ~ThreadPool() {
        wait();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

   private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()> > tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable completion_condition;

    bool stop;
    std::atomic<int> active_tasks;
#endif  // DMZ_SINGLE_THREADED
};
}  // namespace DMZ