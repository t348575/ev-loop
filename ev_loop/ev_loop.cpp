#include "ev_loop.hpp"
#include <thread>
#include <iostream>

using namespace ev;

template <class result_t = std::chrono::microseconds, class clock_t = std::chrono::steady_clock, class duration_t = std::chrono::microseconds>
std::chrono::microseconds since(std::chrono::time_point<clock_t, duration_t> const& start) {
    return std::chrono::duration_cast<result_t>(clock_t::now() - start);
}

auto constexpr range_limit = std::chrono::microseconds(100);
auto constexpr precision = std::chrono::microseconds(950);

bool within(std::chrono::microseconds time_since, std::chrono::microseconds interval) {
    return time_since >= interval - range_limit;
}

EvLoop::EvLoop(u32 num_workers = std::thread::hardware_concurrency()): size(num_workers) {
    if (num_workers == 0) {
        throw std::runtime_error("EvLoop: num_workers must be greater than 0");
    }

    id_counter.store(1);
    for (u32 i = 0; i < num_workers; i++) {
        Worker w(std::make_shared<Worker::SharedData>(i));
        workers[i] = w.shared;
        std::thread(&Worker::Run, w).detach();
    }

    std::thread(&EvLoop::Run, this).detach();
}

void EvLoop::Enqueue(Job j) {
    internal_q.push(QOptions{Type::INSTANT, j});
}

u32 EvLoop::Enqueue(ReoccuringJob j) {
    auto id = id_counter.fetch_add(1);
    j.last_call = clock.now();
    reoccuring_jobs[id] = j;
    return id;
}

void EvLoop::BlockOn(StatusLambda j, u32 id, std::chrono::milliseconds i) {
    while(true) {
        if (reoccuring_jobs.find(id) == reoccuring_jobs.end()) {
            return;
        }
        if (j()) {
            this->StopReccuring(id);
            return;
        }
        std::this_thread::sleep_for(i);
    }
}

std::size_t EvLoop::StopReccuring(u32 id) {
    return reoccuring_jobs.erase(id);
}

void EvLoop::Modify(u32 id, std::chrono::milliseconds i) {
    reoccuring_jobs[id].interval = std::chrono::duration_cast<std::chrono::microseconds>(i);
    reoccuring_jobs[id].last_call = clock.now();
}

void EvLoop::Stop() {
    internal_q.push(QOptions{Type::STOP, Job()});
}

void EvLoop::Run() {
    auto elapsed = since(start);
    bool exit = false;
    while(!exit) {{
            std::lock_guard<std::mutex> lk(internal_mtx);
            for (auto &job: reoccuring_jobs) {
                if (within(since(job.second.last_call), job.second.interval)) {
                    internal_q.push(QOptions{Type::REOCCURING, job.second.j});
                    job.second.last_call = clock.now();
                }
            }
        }

        internal_q.consume_one([&](QOptions item) {
            if (item.type == Type::STOP) {
                exit = true;
                for (u32 i = 0; i < size; i++) {
                    workers[i]->queue.push(item);
                    workers[i]->cv.notify_one();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return;
            }

            auto time_left = since(start);
            if (time_left < precision) {
                std::this_thread::sleep_for(precision - time_left);
            }

            std::lock_guard<std::mutex> lk(internal_mtx);
            u32 smallest = workers[0]->queue_size;
            u32 smallest_idx = 0;
            for (u32 i = 1; i < size; i++) {
                if (workers[i]->queue_size <= smallest) {
                    if (since(workers[i]->curr_start) > precision) {
                        continue;
                    }
                    smallest = workers[i]->queue_size;
                    smallest_idx = i;
                }
            }

            workers[smallest_idx]->queue_size++;
            workers[smallest_idx]->queue.push(item);
            workers[smallest_idx]->cv.notify_one();
        });

        start = clock.now();
    }
}

void Store::Set(u64 key, boost::any value) {
    std::lock_guard<std::recursive_mutex> lock(*mtx);
    store[key] = value;
}

void Store::Remove(u64 key) {
    std::lock_guard<std::recursive_mutex> lock(*mtx);
    store.erase(key);
}

boost::any Store::Get(u64 key) {
    std::lock_guard<std::recursive_mutex> lock(*mtx);
    return store[key];
}

void Worker::Run() {
    std::mutex mtx;
    bool exit = false;
    while (!exit) {
        std::unique_lock<std::mutex> lock(mtx);
        shared->cv.wait(lock, [this]{return !shared->queue.empty();});
        shared->queue.consume_one([&](QOptions item) {
            if (item.type == Type::STOP) {
                exit = true;
                return;
            }
            shared->curr_start = shared->clock.now();
            item.j();
            shared->queue_size--;
        });
        lock.unlock();
    }
}