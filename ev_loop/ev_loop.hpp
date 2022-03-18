#pragma once

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <boost/any.hpp>
#include <boost/optional.hpp>
#include <unordered_map>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>

namespace ev {
    typedef uint32_t u32;
    typedef uint64_t u64;

    class Store {
        public:
            Store() = default;
            void Set(u64, boost::any value);
            void Remove(u64 key);
            boost::any Get(u64 key);

            template<typename Func>
            auto DoLocked(Func func) -> decltype(func(*this)) {
                std::lock_guard<std::recursive_mutex> lock(*mtx);
                return func(*this);
            }
        private:
            std::unique_ptr<std::recursive_mutex> mtx = std::make_unique<std::recursive_mutex>();
            std::unordered_map<u64, boost::any> store;
    };

    typedef boost::optional<Store> OptionalStore;

    class Job;
    typedef std::function<void(Job*)> JobSig;

    class Job {
        public:
            Job() = default;
            Job(u32 id) : id(id) {}
            Job(JobSig func): func(func) {}
            void SetStore(std::shared_ptr<OptionalStore> store_) { store = store_; }
            std::shared_ptr<OptionalStore> GetStore() { return store; }
            void operator()() {
                func(this);
            }
            static Job EmptyJob(u32 id) {
                return Job(id);
            }
        private:
            u32 id = 0;
            JobSig func;
            std::shared_ptr<OptionalStore> store;
    };

    typedef boost::lockfree::spsc_queue<Job, boost::lockfree::capacity<128>> WorkQ;

    struct Worker {
        struct SharedData {
            u32 id;
            u32 queue_size;
            WorkQ queue;
            std::condition_variable cv;
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

    class EvLoop {
        public:
            EvLoop(u32 num_workers);
            void Enqueue(Job j);
            u32 Enqueue(ReoccuringJob j);
            std::size_t StopReccuring(u32 id);
            void Modify(u32 id, std::chrono::milliseconds i);
            void Run();
        private:
            struct QOptions {
                enum type {
                    INSTANT,
                    REOCCURING
                } type_;
                Job j;
            };
            u32 size;
            std::atomic_uint32_t id_counter;
            std::mutex internal_mtx;
            std::unordered_map<u32, std::shared_ptr<Worker::SharedData>> workers;
            boost::lockfree::spsc_queue<Job, boost::lockfree::capacity<512>> internal_q;
            std::unordered_map<u32, ReoccuringJob> reoccuring_jobs;
            std::chrono::steady_clock clock = std::chrono::steady_clock();
            std::chrono::steady_clock::time_point start;
    };
}