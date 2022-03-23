#include <iostream>
#include <thread>
#include "ev_loop/ev_loop.hpp"

int main() {
    ev::EvLoop my_event_loop(2); // event loop with 2 workers

    // create a data store for input
    auto data_store = std::make_shared<boost::optional<ev::Store>>(ev::Store());
    data_store->get().Set(0, boost::any(10));

    // queue a simple sum job
    for(int i = 0; i < 10; i++) {
        ev::Job j([id=i](ev::Job *self) {
            std::cout << "Executing job: " << std::to_string(id) << std::endl;
            self->GetStore()->get().DoLocked([id](ev::Store &s) {
                int result = boost::any_cast<int>(s.Get(0)) + id;
                s.Set(id + 1, boost::any(result));
            });
        });
        j.SetStore(data_store);
        my_event_loop.Enqueue(j);
    }

    // run task every 1.5 seconds
    ev::Job j_reoccuring([](ev::Job *self) { std::cout << "Every 1.5 seconds!" << std::endl; });
    my_event_loop.Enqueue(ev::ReoccuringJob(j_reoccuring, std::chrono::milliseconds(1500)));

    std::this_thread::sleep_for(std::chrono::seconds(5));

    for (int i = 0; i < 10; i++) {
        std::cout << "Sum result of worker " << i << " is " << boost::any_cast<int>(data_store->get().Get(i + 1)) << std::endl;
    }

    // stop the event loop
    my_event_loop.Stop();
    return 0;
}