#include "TCPConnectionManager.h"
#include "GameLogic/Match.h"

#include <thread>
#include <chrono>

int main_func(){
    std::cout << "High-Performance UDP Game Server with Memory Pool Optimization\n";
    std::cout << "Compilation: g++ -std=c++17 -O3 -pthread -I/path/to/concurrentqueue game_server.cpp -o game_server\n";
    int tcp_port = 8112;

    auto tcp_manager = std::make_unique<TCPConnectionManager>();
    // Initialize TCP manager
    if (!tcp_manager->initialize(tcp_port)) {
        std::cerr << "Failed to initialize TCP manager" << std::endl;
        return 0;
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

int test(){
    AnCom::Match* match = new AnCom::Match();

    // Spawn 5 slimes
    for (int i = 0; i < 5; i++) {
        float x = 100.0f + (i * 100.0f);
        float y = 100.0f;
        match->SpawnSlime(x, y, 1000 + i);
    }

    // Game loop
    while (true) {
        match->Update();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [test|real]\n";
        return 0;
    }

    std::string mode = argv[1];

    if (mode == "test") {
        std::cout << "Running in TEST mode...\n";
        return test();
    } 
    else if (mode == "real") {
        std::cout << "Running in REAL SERVER mode...\n";
        return main_func();
    } 
    else {
        std::cerr << "Unknown mode: " << mode << "\n";
        std::cout << "Usage: " << argv[0] << " [test|real]\n";
        return 1;
    }
}