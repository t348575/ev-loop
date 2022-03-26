#pragma once

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <boost/any.hpp>
#include <unordered_map>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>

namespace ev {
    typedef uint32_t u32;
    typedef uint64_t u64;

    template<typename T, typename ...Args>
    std::unique_ptr<T> make_unique(Args&& ...args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    struct Store {
        Store() = default;
        void Set(u64, boost::any value);
        void Remove(u64 key);
        boost::any Get(u64 key);

        template<typename Func>
        auto DoLocked(Func func) -> decltype(func(*this)) {
            std::lock_guard<std::recursive_mutex> lock(*mtx);
            return func(*this);
        }
        std::unique_ptr<std::recursive_mutex> mtx = make_unique<std::recursive_mutex>();
        std::unordered_map<u64, boost::any> store;
    };

    class Job;
    typedef std::function<void(Job*)> JobSig;

    enum Type {
        INSTANT,
        REOCCURING,
        STOP
    };

    class Job {
        public:
            Job() = default;
            Job(JobSig func): func(func) {}
            void SetStore(Store *store_) { store = store_; }
            Store* GetStore() { return store; }
            void operator()() {
                func(this);
            }
        private:
            u32 id = 0;
            JobSig func;
            Store *store;
    };

    struct QOptions {
        Type type;
        Job j;
    };

    typedef boost::lockfree::spsc_queue<QOptions, boost::lockfree::capacity<128>> WorkQ;

    struct Worker {
        struct SharedData {
            u32 id;
            u32 queue_size;
            WorkQ queue;
            std::condition_variable cv;
            std::chrono::steady_clock::time_point curr_start;
            std::chrono::steady_clock clock = std::chrono::steady_clock();
            SharedData(u32 id): id(id) { queue_size = 0; }
        };
        std::shared_ptr<SharedData> shared;
        Worker(std::shared_ptr<SharedData> shared): shared(shared) {}
        void Run();
    };

    struct ReoccuringJob {
        ReoccuringJob(Job j, std::chrono::milliseconds i): j(j) {
            interval = std::chrono::duration_cast<std::chrono::microseconds>(i);
        }
        ReoccuringJob() = default;
        Job j;
        std::chrono::microseconds interval;
        std::chrono::steady_clock::time_point last_call;
    };

    class EvLoop;
    typedef std::function<bool(EvLoop*)> StatusLambda;

    class EvLoop {
        public:
            EvLoop(u32 num_workers);
            void Enqueue(Job j);
            u32 Enqueue(ReoccuringJob j);
            void BlockOn(StatusLambda j, u32, std::chrono::milliseconds i);
            std::size_t StopReccuring(u32 id);
            void Modify(u32 id, std::chrono::milliseconds i);
            void Run();
            void Stop();
        private:
            u32 size;
            std::atomic_uint32_t id_counter;
            std::mutex internal_mtx;
            std::unordered_map<u32, std::shared_ptr<Worker::SharedData>> workers;
            boost::lockfree::spsc_queue<QOptions, boost::lockfree::capacity<512>> internal_q;
            std::unordered_map<u32, ReoccuringJob> reoccuring_jobs;
            std::chrono::steady_clock clock = std::chrono::steady_clock();
            std::chrono::steady_clock::time_point start;
    };
}