#include "TCPConnectionManager.h"

int main() {
    std::cout << "High-Performance UDP Game Server with Memory Pool Optimization\n";
    std::cout << "Compilation: g++ -std=c++17 -O3 -pthread -I/path/to/concurrentqueue game_server.cpp -o game_server\n";
    int tcp_port = 8112;

    auto tcp_manager = std::make_unique<TCPConnectionManager>();
    // Initialize TCP manager
    if (!tcp_manager->initialize(tcp_port)) {
        std::cerr << "Failed to initialize TCP manager" << std::endl;
        return false;
    }
    // Initialize TCP manager
    std::thread tcp_thread([&tcp_manager]() {
        tcp_manager->start();
    });

    HighPerformanceGameServer server;
    
    if (!server.initialize(8080)) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    std::cout << "Starting high-performance game server with memory pool optimization..." << std::endl;
    
    // Handle Ctrl+C gracefully
    std::thread signal_thread([&server]() {
        std::cin.get();
        std::cout << "Shutting down server..." << std::endl;
        server.stop();
    });
    
    server.start();
    signal_thread.join();
    
    return 0;
}