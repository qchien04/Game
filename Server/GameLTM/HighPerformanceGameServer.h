#include "common.h"
#include "MemoryPool.h"
#include "GameLogic/Match.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// Using moodycamel's high-performance concurrent queue with pointers for zero-copy
using MessageQueue = moodycamel::ConcurrentQueue<GameMessage*>;

class HighPerformanceGameServer {
private:
    SOCKET server_socket;
    HANDLE iocp_handle;

    std::atomic<bool> running;
    
    // Thread pools
    std::vector<std::thread> network_threads;
    std::vector<std::thread> game_logic_threads;
    std::vector<std::thread> broadcast_threads;
    
    // High-performance concurrent queues for inter-thread communication (using pointers)
    std::vector<std::unique_ptr<MessageQueue>> incoming_queues;
    std::vector<std::unique_ptr<MessageQueue>> outgoing_queues;
    
    // Per-thread tokens for better performance
    std::vector<moodycamel::ProducerToken> incoming_producer_tokens;
    std::vector<moodycamel::ConsumerToken> incoming_consumer_tokens;

    std::vector<moodycamel::ProducerToken> outgoing_producer_tokens;
    std::vector<moodycamel::ConsumerToken> outgoing_consumer_tokens;
    
    // Player management
    std::unordered_map<uint32_t, std::unique_ptr<Player>> players;
    std::atomic<uint32_t> next_player_id;
    
    // Memory pools
    MemoryPool<GameMessage> message_pool;
    
    // Performance metrics
    std::atomic<uint64_t> messages_processed;
    std::atomic<uint64_t> messages_sent;
    std::atomic<uint64_t> active_connections;
    std::atomic<uint64_t> pool_allocations;
    std::atomic<uint64_t> pool_deallocations;
    
public:
    static constexpr int MAX_EVENTS = 10000;
    static constexpr int NETWORK_THREAD_COUNT = 4;
    static constexpr int GAME_LOGIC_THREAD_COUNT = 1;
    static constexpr int MATCH_SHARD_COUNT = 4;
    static constexpr int MATCH_SHARD_NUMBER_PER_THREAD = 4;

    inline static std::array<std::unordered_map<int, std::unique_ptr<AnCom::Match>>, MATCH_SHARD_COUNT> match_shards;
    static constexpr int BROADCAST_THREAD_COUNT = 2;
    static constexpr int BUFFER_SIZE = 65536;

    HighPerformanceGameServer() : 
        server_socket(INVALID_SOCKET),
        iocp_handle(NULL),

        running(false),
        next_player_id(1),
        messages_processed(0),
        messages_sent(0),
        active_connections(0),
        pool_allocations(0),
        pool_deallocations(0) {

        // Initialize Winsock
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            std::cerr << "WSAStartup failed\n";
            throw std::runtime_error("WSAStartup failed");
        }
        
        // Initialize queues with pre-allocated capacity for better performance
        for (int i = 0; i < NETWORK_THREAD_COUNT; ++i) {
            incoming_queues.push_back(std::make_unique<MessageQueue>(static_cast<size_t>(16384)));
            outgoing_queues.push_back(std::make_unique<MessageQueue>(static_cast<size_t>(16384)));
        }

        // Initialize tokens for each thread (improves performance significantly)
        for (int i = 0; i < NETWORK_THREAD_COUNT; ++i) {
            incoming_producer_tokens.emplace_back(*incoming_queues[i]);
            incoming_consumer_tokens.emplace_back(*incoming_queues[i]);
            outgoing_producer_tokens.emplace_back(*outgoing_queues[i]);
            outgoing_consumer_tokens.emplace_back(*outgoing_queues[i]);
        }
    }
    
    void CreateMatch(){
        auto test_match = std::make_unique<AnCom::Match>();

        test_match->AddVirtualPlayer(1);
        test_match->AddVirtualPlayer(2);

        int shard_to_add = test_match->GetMatchId() % MATCH_SHARD_COUNT;
        test_match->PlayerAttack(1,1,1);
        std::cout<<"[CREATE]"<<test_match->GetMatchId()<<" "<<shard_to_add<<" "<<std::endl;

        match_shards[shard_to_add][test_match->GetMatchId()] = std::move(test_match);
    }

    bool initialize(int port) {
        // Create UDP socket
        server_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (server_socket == INVALID_SOCKET) {
            std::cerr << "Failed to create socket: " << WSAGetLastError() << "\n";
            return false;
        }
        
        // Set socket to non-blocking mode
        u_long mode = 1;
        if (ioctlsocket(server_socket, FIONBIO, &mode) != 0) {
            std::cerr << "Failed to set non-blocking mode\n";
            closesocket(server_socket);
            return false;
        }
        
        // Set socket options for high performance
        BOOL opt = TRUE;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        // Increase socket buffer sizes
        int buffer_size = 16 * 1024 * 1024; // 16MB
        setsockopt(server_socket, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(buffer_size));
        setsockopt(server_socket, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, sizeof(buffer_size));
        
        // Bind socket
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to bind socket: " << WSAGetLastError() << "\n";
            closesocket(server_socket);
            return false;
        }
        
        // Create IOCP handle (Windows alternative to epoll)
        iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (iocp_handle == NULL) {
            std::cerr << "Failed to create IOCP\n";
            closesocket(server_socket);
            return false;
        }
        
        // Associate socket with IOCP
        if (CreateIoCompletionPort((HANDLE)server_socket, iocp_handle, (ULONG_PTR)server_socket, 0) == NULL) {
            std::cerr << "Failed to associate socket with IOCP\n";
            closesocket(server_socket);
            CloseHandle(iocp_handle);
            return false;
        }
        
        std::cout << "Server initialized on port " << port << std::endl;
        return true;
    }
    
    void start() {
        running.store(true);
        
        // Start network threads
        for (int i = 0; i < NETWORK_THREAD_COUNT; ++i) {
            network_threads.emplace_back(&HighPerformanceGameServer::network_thread, this, i);
        }
        
        // Start game logic threads
        for (int i = 0; i < GAME_LOGIC_THREAD_COUNT; ++i) {
            game_logic_threads.emplace_back(&HighPerformanceGameServer::game_logic_thread, this, i);
        }
        
        // Start broadcast threads
        for (int i = 0; i < BROADCAST_THREAD_COUNT; ++i) {
            broadcast_threads.emplace_back(&HighPerformanceGameServer::broadcast_thread, this, i);
        }
        
        // Performance monitoring thread
        std::thread monitor_thread(&HighPerformanceGameServer::monitor_performance, this);
        
        std::cout << "Game server started with " 
                  << NETWORK_THREAD_COUNT << " network threads, "
                  << GAME_LOGIC_THREAD_COUNT << " game logic threads, "
                  << BROADCAST_THREAD_COUNT << " broadcast threads\n";
        std::cout << "Message pool initialized with capacity: " << message_pool.get_total_capacity() << "\n";
        
        // Main event loop
        main_event_loop();
        
        // Wait for all threads
        for (auto& t : network_threads) t.join();
        for (auto& t : game_logic_threads) t.join();
        for (auto& t : broadcast_threads) t.join();
        monitor_thread.join();
    }
    
    void stop() {
        running.store(false);
    }

    ~HighPerformanceGameServer() {
        if (server_socket != INVALID_SOCKET) closesocket(server_socket);
        if (iocp_handle != NULL) CloseHandle(iocp_handle);
        WSACleanup();
    }
    
private:
    void main_event_loop() {
        std::array<uint8_t, BUFFER_SIZE> buffer;
        
        while (running.load()) {
            // Windows: Use select or polling for UDP
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_socket, &read_fds);
            
            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 1000; // 1ms
            
            int activity = select(0, &read_fds, NULL, NULL, &timeout);
            
            if (activity == SOCKET_ERROR) {
                if (WSAGetLastError() != WSAEINTR) {
                    std::cerr << "Select error: " << WSAGetLastError() << std::endl;
                }
                continue;
            }
            
            if (activity > 0 && FD_ISSET(server_socket, &read_fds)) {
                handle_incoming_data(buffer);
            }
        }
    }
    
    void handle_incoming_data(std::array<uint8_t, BUFFER_SIZE>& buffer) {
        sockaddr_in client_addr;

        int addr_len = sizeof(client_addr);
        
        while (true) {
            int bytes_received = recvfrom(server_socket, (char*)buffer.data(), BUFFER_SIZE, 
                                         0, (sockaddr*)&client_addr, &addr_len);
            
            if (bytes_received == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    break; // No more data
                }
                continue;
            }
            
            if (bytes_received < sizeof(uint32_t) + sizeof(uint16_t)) {
                continue; // Invalid message
            }
            
            // Acquire message from pool instead of creating on stack
            GameMessage* msg = message_pool.acquire();
            if (!msg) {
                std::cerr << "Failed to acquire message from pool\n";
                continue;
            }
            
            pool_allocations.fetch_add(1);
            msg->reset(); // Ensure clean state
            
            uint8_t* ptr = buffer.data();

            // Read 4 bytes big-endian → host-order
            int32_t net_player_id = *reinterpret_cast<int32_t*>(ptr);
            msg->player_id = ntohl(net_player_id);
            ptr += sizeof(int32_t);

            // Read 4 bytes big-endian → host-order
            int32_t net_msg_type = *reinterpret_cast<int32_t*>(ptr);
            msg->msg_type = ntohl(net_msg_type);
            ptr += sizeof(int32_t);

            // Read 4 bytes timestamp
            int32_t net_match_id = *reinterpret_cast<int32_t*>(ptr);
            msg->match_id = ntohl(net_match_id);
            ptr += sizeof(int32_t);

            msg->client_addr = client_addr;

            // Payload
                msg->payload_size = bytes_received - (sizeof(int32_t) * 3);
                if (msg->payload_size > 0) {
                    memcpy(msg->payload.data(), ptr, (std::min)(msg->payload_size, msg->payload.size()));
                }
            
            // Distribute to network threads using round-robin
            static std::atomic<int> thread_index(0);
            int idx = thread_index.fetch_add(1) % NETWORK_THREAD_COUNT;
            std::cout << "[ENQUEUE] thread_idx:" << idx 
                        << " player_id:" << msg->player_id 
                        << " match_id:"<<msg->match_id
                        << " ptr:" << msg << std::endl;

            if (!incoming_queues[idx]) {
                std::cout << "incoming_queues[" << idx << "] is null!\n";
                abort();
            }
            if (!msg) {
                std::cout << "Message is null\n";
                abort();
            }
            incoming_queues[idx]->enqueue(incoming_producer_tokens[idx], msg);
            
            messages_processed.fetch_add(1);
        }
    }
    
    void network_thread(int thread_id) {        
        // Batch processing arrays for better performance (using pointers)
        std::array<GameMessage*, 256> incoming_batch;
        
        while (running.load()) {
            bool found_work = false;
            
            // Process incoming messages in batches
            size_t dequeued = incoming_queues[thread_id]->try_dequeue_bulk(incoming_consumer_tokens[thread_id], 
                                                                          incoming_batch.begin(), 
                                                                          incoming_batch.size());
            if (dequeued > 0) {                
                for (size_t i = 0; i < dequeued; ++i) {
                    process_message(*incoming_batch[i]);

                    int match_id = static_cast<int>(incoming_batch[i]->match_id);
                    int shard_worker = match_id % MATCH_SHARD_COUNT;
                    
                    auto it = match_shards[shard_worker].find(match_id);
                    if (it != match_shards[shard_worker].end()) {
                        AnCom::PlayerAction action = AnCom::Match::DePayload(
                            incoming_batch[i]->player_id,
                            incoming_batch[i]->payload.data(),
                            incoming_batch[i]->payload_size);
                        it->second->EnqueuePlayerAction(action);
                    }

                    message_pool.release(incoming_batch[i]);
                    pool_deallocations.fetch_add(1);
                }
                found_work = true;
            }
        }
    }
    
    void game_logic_thread(int thread_id) {
        const auto update_interval = std::chrono::milliseconds(33); // ~60 FPS
        while (running.load()) {
            for(size_t shard = thread_id; shard < MATCH_SHARD_COUNT; shard += GAME_LOGIC_THREAD_COUNT){
                for (auto it = match_shards[shard].begin(); it != match_shards[shard].end(); it++) {
                    auto now = std::chrono::steady_clock::now();
                    
                    if (now - it->second->last_update >= update_interval) {
                        it->second->Update();
                        it->second->last_update = now;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void broadcast_thread(int thread_id) {
        const auto broadcast_interval = std::chrono::milliseconds(33); // ~60 FPS
        while (running.load()) {
            for(size_t shard = thread_id; shard < MATCH_SHARD_COUNT; shard += GAME_LOGIC_THREAD_COUNT){
                for (auto it = match_shards[shard].begin(); it != match_shards[shard].end(); it++) {
                    auto now = std::chrono::steady_clock::now();
                    
                    if (now - it->second->last_broadcast >= broadcast_interval) {
                        it->second->last_broadcast = now;
                            
                        std::array<uint8_t, BUFFER_SIZE> buffer;
                        uint8_t* ptr = buffer.data();

                        size_t total_size;
                        it->second->SerializeGameState(ptr, total_size);
                        
                        for(size_t player = 0; player < it->second->GetAllPlayer().size(); player++){
                            auto p = players.find(it->second->GetAllPlayer()[player]->GetId());
                            if (p != players.end()) {
                                int sent = sendto(server_socket, (char*)buffer.data(), total_size, 
                                               0, (sockaddr*)&p->second->addr, sizeof(p->second->addr));
                                if (sent > 0) {
                                    messages_sent.fetch_add(1);
                                }
                            }
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void process_message(const GameMessage& msg) {
        switch (msg.msg_type) {
            case 1: // Join game
                handle_player_join(msg);
                break;
            case 2: // Player movement
                handle_player_movement(msg);
                break;
            case 3: // Player action
                handle_player_action(msg);
                break;
            case 99: // Disconnect
                handle_player_disconnect(msg);
                break;
        }
    }
    
    void handle_player_join(const GameMessage& msg) {
        uint32_t player_id = next_player_id.fetch_add(1);
        
        auto player = std::make_unique<Player>();
        player->id = msg.player_id;
        player->addr = msg.client_addr;
        player->last_seen = std::chrono::steady_clock::now();
        player->active = true;
        
        auto it = players.find(msg.player_id);
        if (it == players.end()) {
            players[msg.player_id] = std::move(player);
            active_connections.fetch_add(1);
        }
    }
    
    void handle_player_movement(const GameMessage& msg) {
        auto it = players.find(msg.player_id);
        if (it != players.end() && msg.payload_size >= sizeof(float) * 3) {
            const float* pos = reinterpret_cast<const float*>(msg.payload.data());
            it->second->last_seen = std::chrono::steady_clock::now();
        }
    }
    
    void handle_player_action(const GameMessage& msg) {
        auto it = players.find(msg.player_id);
        if (it != players.end()) {
            it->second->last_seen = std::chrono::steady_clock::now();
        }
    }
    
    void handle_player_disconnect(const GameMessage& msg) {
        auto it = players.find(msg.player_id);
        if (it != players.end()) {
            players.erase(it);
            active_connections.fetch_sub(1);
        }
    }
    
    void monitor_performance() {
        auto last_check = std::chrono::steady_clock::now();
        uint64_t last_messages_processed = 0;
        uint64_t last_messages_sent = 0;
        uint64_t last_allocations = 0;
        uint64_t last_deallocations = 0;
        
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(100));
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_check).count();
            
            uint64_t current_processed = messages_processed.load();
            uint64_t current_sent = messages_sent.load();
            uint64_t current_allocations = pool_allocations.load();
            uint64_t current_deallocations = pool_deallocations.load();
            
            if (elapsed > 0) {
                uint64_t processed_rate = (current_processed - last_messages_processed) / elapsed;
                uint64_t sent_rate = (current_sent - last_messages_sent) / elapsed;
                uint64_t alloc_rate = (current_allocations - last_allocations) / elapsed;
                uint64_t dealloc_rate = (current_deallocations - last_deallocations) / elapsed;
                
                std::cout << "Performance Stats:\n"
                         << "  Active Players: " << active_connections.load() << "\n"
                         << "  Messages/sec (In): " << processed_rate << "\n"
                         << "  Messages/sec (Out): " << sent_rate << "\n"
                         << "  Pool Allocs/sec: " << alloc_rate << "\n"
                         << "  Pool Deallocs/sec: " << dealloc_rate << "\n"
                         << "  Pool Active Objects: " << message_pool.get_allocated_count() << "\n"
                         << "  Pool Total Capacity: " << message_pool.get_total_capacity() << "\n"
                         << "  Total Processed: " << current_processed << "\n"
                         << "  Total Sent: " << current_sent << "\n"
                         << std::endl;
            }
            
            last_check = now;
            last_messages_processed = current_processed;
            last_messages_sent = current_sent;
            last_allocations = current_allocations;
            last_deallocations = current_deallocations;
        }
    }
};