#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <atomic>


namespace spdlog {
class logger;
}

namespace Afina {
namespace Concurrency {

class Executor;
void perform(Executor *executor);


/**
 * # Thread pool
 */
class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

public:
    //low_watermark - минимальное количество потоков, которое должно быть в пуле.
    //hight_watermark - максимальное количество потоков в пуле
    //max_queue_size - максимальное число задач в очереди
    //idle_time - количество миллисекунд, которое каждый из поток ждет задач.
    Executor(int low_watermark, int hight_watermark, int max_queue_size, int idle_time);
    ~Executor();

    /**
     * Start thread-pool
     */
    void Start(std::shared_ptr<spdlog::logger> logger);

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            if (state != State::kRun || tasks.size() >= max_queue_size) {
                return false;
            }

            // Enqueue new task
            tasks.push_back(exec);
            if (free_threads == 0 && threads.size() < hight_watermark) {
                threads.push_back(std::thread(&perform, this));
            }
            empty_condition.notify_one();
        }
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor);

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Conditional variable to await for server to stop
     */
    std::condition_variable stop_condition;

    /**
     * Vector of actual threads that perform execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    const int low_watermark;
    const int hight_watermark;
    const int max_queue_size;
    const int idle_time;
    int free_threads;
    std::shared_ptr<spdlog::logger> _logger;

    void _erase_thread();

};


} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
