#include <iostream>
#include <thread>
#include "ev_loop/ev_loop.h"

int main() {
    ev::EvLoop my_event_loop(8);

    // my_event_loop.enqueue("Hello, world!");
    auto data_store = std::make_shared<boost::optional<ev::Store>>(ev::Store());
    for (int i = 0; i < 100; i++) {
        data_store->get().Set(std::to_string(i), boost::any(i + 1));
    }

    // for(int i = 0; i < 10; i++) {
    //     ev::Job j([id=i](ev::Job *self) {
    //         // int sum = 0;
    //         // for(int i = 0; i < 1000; i++) {
    //         //     sum += (i * id);
    //         // }
    //         // self->GetIn()->get().DoLocked([&sum, &id](ev::Store &s) {
    //         //     s.Set(std::to_string(id), sum);
    //         // });
    //         std::string req = std::to_string(id);
    //         std::cout << "In job: " << req << std::endl;
    //         std::cout << "Hello, world!\ninput data: " << boost::any_cast<int>(self->GetIn()->get().Get(req)) << std::endl;
    //         self->GetIn()->get().DoLocked([](ev::Store &s) {
    //             std::cout << "DoLocked: " << boost::any_cast<int>(s.Get("1")) << std::endl;
    //             return;
    //         });
    //     });
    //     j.SetIn(data_store);
    //     my_event_loop.Enqueue(j);
    //     // std::thread([&my_event_loop, &data_store, i]() {
    //     // }).join();
    // }

    ev::Job j_one([](ev::Job *self) {
        std::cout << "every one second" << std::endl;
    });
    ev::Job j_three([](ev::Job *self) {
        std::cout << "every three second" << std::endl;
    });
    my_event_loop.Enqueue(ev::ReoccuringJob(j_one, std::chrono::milliseconds(1000)));
    my_event_loop.Enqueue(ev::ReoccuringJob(j_three, std::chrono::milliseconds(3000)));

    std::this_thread::sleep_for(std::chrono::seconds(10000));
    return 0;
}