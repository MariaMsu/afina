#include <afina/concurrency/Executor.h>
#include <algorithm>

namespace Afina {
namespace Concurrency {

Executor::Executor(int low_watermark, int hight_watermark, int max_queue_size, int idle_time) :
        low_watermark(low_watermark),
        hight_watermark(hight_watermark),
        max_queue_size(max_queue_size),
        idle_time(idle_time) {}

Executor::~Executor() {}

void Executor::Start(std::shared_ptr<spdlog::logger> logger) {
    _logger = std::move(logger);
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kRun;
    for (int i = 0; i < low_watermark; i++) {
        threads.push_back(std::thread(&perform, this));
    }
    free_threads = 0;
}

void Executor::Stop(bool await) {
    if (state == State::kStopped) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kStopping;
    empty_condition.notify_all();
    if (await) {
        stop_condition.wait(lock, [this]() { return (this->state == Executor::State::kStopped); });
    }
    state = State::kStopped;
}

void perform(Executor *executor) {
    // new thread
    while (executor->state == Executor::State::kRun) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(executor->mutex);
            auto time_until = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->idle_time);
            while (executor->tasks.empty() && executor->state == Executor::State::kRun) {
                // waiting
                executor->free_threads++;
                if (executor->empty_condition.wait_until(lock, time_until) == std::cv_status::timeout) {
                    if (executor->threads.size() > executor->low_watermark) {
                        executor->_erase_thread();
                        return;
                    } else {
                        executor->empty_condition.wait(lock);
                    }
                }
                executor->free_threads--;
            }
            // stop waiting
            if (executor->tasks.empty()) {
                continue;
            }
            task = executor->tasks.front();
            executor->tasks.pop_front();
        }
        task();
    }
    {
        std::unique_lock<std::mutex> lock(executor->mutex);
        executor->_erase_thread();
        if (executor->threads.empty()) {
            executor->stop_condition.notify_all();
        }
    }
}

void Executor::_erase_thread() {
    std::thread::id cur_thread_id = std::this_thread::get_id();
    auto iter = std::find_if(threads.begin(), threads.end(), [=](std::thread &t) { return (t.get_id() == cur_thread_id); });
    if (iter != threads.end()) {
        iter->detach();
        free_threads--;
        threads.erase(iter);
        return;
    }
    throw std::runtime_error("error while erasing thread");
}


} // namespace Concurrency
} // namespace Afina
