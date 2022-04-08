#include <iostream>
#include <thread>
#include "ev_loop/ev_loop.hpp"

int main() {
    std::shared_ptr<ev::EvLoop> my_event_loop = ev::make_unique<ev::EvLoop>(4); // 12 workers

    // create a data store for input
    ev::Store * data_store = new ev::Store();
    data_store->Set(0, boost::any(10));

    // queue a simple sum job
    for(int i = 0; i < 5; i++) {
        ev::Job j([i, my_event_loop](ev::Job *self) {
            std::cout << "Executing job: " << std::to_string(i) << std::endl;
            self->GetStore()->DoLocked([i](ev::Store &s) {
                s.store[i + 1] = boost::any_cast<int>(s.store[0]) + i;
            });
        });
        j.SetStore(data_store);
        my_event_loop->Enqueue(j);
    }

    // run task every 1.5 seconds
    ev::Job j_reoccuring([](ev::Job *self) { std::cout << "Every 1.5 seconds!" << std::endl; });
    auto id = my_event_loop->Enqueue(ev::ReoccuringJob(j_reoccuring, std::chrono::milliseconds(1500)));

    std::thread([&my_event_loop, &id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        my_event_loop->StopReccuring(id);
    }).detach();

    my_event_loop->BlockOn([]() { return false; }, id, std::chrono::milliseconds(1500));


    for (int i = 0; i < 5; i++) {
        std::cout << "Sum result of worker " << i << " is " << boost::any_cast<int>(data_store->Get(i + 1)) << std::endl;
    }

    // stop the event loop
    my_event_loop->Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return 0;
}