#include "TCPConnectionManager.h"
#include <thread>
#include <iostream>
#include <memory>

int main() {
    int tcp_port = 8112;

    auto tcp_manager = std::make_unique<TCPConnectionManager>();

    if (!tcp_manager->initialize(tcp_port)) {
        std::cerr << "Failed to initialize TCP manager" << std::endl;
        return 0;
    }

    std::thread tcp_thread([&tcp_manager]() {
        tcp_manager->start();
    });

    tcp_thread.join(); 
}
