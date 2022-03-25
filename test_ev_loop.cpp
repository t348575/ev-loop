#include <iostream>
#include <thread>
#include "ev_loop/ev_loop.hpp"

int main() {
    ev::EvLoop my_event_loop(12); // event loop with 2 workers

    // create a data store for input
    ev::Store * data_store = new ev::Store();
    data_store->Set(0, boost::any(10));

    // queue a simple sum job
    for(int i = 0; i < 10; i++) {
        ev::Job j([id=i](ev::Job *self) {
            std::cout << "Executing job: " << std::to_string(id) << std::endl;
            self->GetStore()->DoLocked([id](ev::Store &s) {
                s.store[id + 1] = boost::any_cast<int>(s.store[0]) + id;
            });
        });
        j.SetStore(data_store);
        my_event_loop.Enqueue(j);
    }

    // run task every 1.5 seconds
    ev::Job j_reoccuring([](ev::Job *self) { std::cout << "Every 1.5 seconds!" << std::endl; });
    auto id = my_event_loop.Enqueue(ev::ReoccuringJob(j_reoccuring, std::chrono::milliseconds(1500)));

    std::thread([&my_event_loop, &id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        my_event_loop.StopReccuring(id);
    }).detach();

    my_event_loop.BlockOn(id, std::chrono::milliseconds(1500));


    for (int i = 0; i < 10; i++) {
        std::cout << "Sum result of worker " << i << " is " << boost::any_cast<int>(data_store->Get(i + 1)) << std::endl;
    }

    // stop the event loop
    my_event_loop.Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return 0;
}