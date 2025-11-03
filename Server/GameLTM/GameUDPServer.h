#pragma once
#include "common.h"
#include "MemoryPool.h"
#include "GameLogic/Match.h"
#include "NetWork/UDPNetWorkDualChannel.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using MessageQueue = moodycamel::ConcurrentQueue<GameMessage*>;

class GameUDPServer {
private:
    SOCKET server_socket;
    HANDLE iocp_handle;

    std::atomic<bool> running;
    
    // Dual-channel UDP system
    std::unique_ptr<UDPNetWorkDualChannel> dual_channel;
    
    // Thread pools
    std::vector<std::thread> network_threads;
    std::vector<std::thread> game_logic_threads;
    std::vector<std::thread> broadcast_threads;
    
    // High-performance concurrent queues for inter-thread communication
    std::vector<std::unique_ptr<MessageQueue>> incoming_queues;
    
    // Per-thread tokens for better performance
    std::vector<moodycamel::ProducerToken> incoming_producer_tokens;
    std::vector<moodycamel::ConsumerToken> incoming_consumer_tokens;
    
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
    static constexpr int BROADCAST_THREAD_COUNT = 2;
    static constexpr int BUFFER_SIZE = 65536;

    inline static std::array<std::unordered_map<int, std::unique_ptr<AnCom::Match>>, MATCH_SHARD_COUNT> match_shards;

    GameUDPServer() : 
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
        
        // Initialize queues with pre-allocated capacity
        for (int i = 0; i < NETWORK_THREAD_COUNT; ++i) {
            incoming_queues.push_back(std::make_unique<MessageQueue>(static_cast<size_t>(16384)));
        }

        // Initialize tokens for each thread
        for (int i = 0; i < NETWORK_THREAD_COUNT; ++i) {
            incoming_producer_tokens.emplace_back(*incoming_queues[i]);
            incoming_consumer_tokens.emplace_back(*incoming_queues[i]);
        }
    }
    
    void CreateMatch(){
        auto test_match = std::make_unique<AnCom::Match>();

        test_match->AddVirtualPlayer(1);
        test_match->AddVirtualPlayer(2);

        int shard_to_add = test_match->GetMatchId() % MATCH_SHARD_COUNT;
        test_match->PlayerAttack(1,1,1);
        for (int i = 0; i < 5; i++) {
            float x = 100.0f + (i * 100.0f);
            float y = 100.0f;
            test_match->SpawnSlime(x, y, 1000 + i);
        }
        std::cout<<"[CREATE] Match:" << test_match->GetMatchId() 
                 << " Shard:" << shard_to_add << std::endl;

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
        
        // Initialize dual-channel system
        dual_channel = std::make_unique<UDPNetWorkDualChannel>(server_socket);
        dual_channel->start();
        
        std::cout << "Server initialized on port " << port << std::endl;
        std::cout << "Dual-channel system started\n";
        return true;
    }
    
    void start() {
        running.store(true);
        
        // Start network threads
        for (int i = 0; i < NETWORK_THREAD_COUNT; ++i) {
            network_threads.emplace_back(&GameUDPServer::network_thread, this, i);
        }
        
        // Start game logic threads
        for (int i = 0; i < GAME_LOGIC_THREAD_COUNT; ++i) {
            game_logic_threads.emplace_back(&GameUDPServer::game_logic_thread, this, i);
        }
        
        // Start broadcast threads
        for (int i = 0; i < BROADCAST_THREAD_COUNT; ++i) {
            broadcast_threads.emplace_back(&GameUDPServer::broadcast_thread, this, i);
        }
        
        // Performance monitoring thread
        std::thread monitor_thread(&GameUDPServer::monitor_performance, this);
        
        std::cout << "Game server started:\n"
                  << "  Network threads: " << NETWORK_THREAD_COUNT << "\n"
                  << "  Game logic threads: " << GAME_LOGIC_THREAD_COUNT << "\n"
                  << "  Broadcast threads: " << BROADCAST_THREAD_COUNT << "\n"
                  << "  Message pool capacity: " << message_pool.get_total_capacity() << "\n";
        
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
        if (dual_channel) {
            dual_channel->stop();
        }
    }

    ~GameUDPServer() {
        stop();
        if (server_socket != INVALID_SOCKET) closesocket(server_socket);
        if (iocp_handle != NULL) CloseHandle(iocp_handle);
        WSACleanup();
    }
    
private:
    void main_event_loop() {
        std::array<uint8_t, BUFFER_SIZE> buffer;
        
        while (running.load()) {
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
            
            if (bytes_received < 1) continue;
            
            // Process through dual-channel system first
            if (!dual_channel->process_incoming(buffer.data(), bytes_received, client_addr)) {
                continue; // Duplicate or ACK packet
            }
            
            // Determine message type
            uint8_t flag = buffer[0];
            bool is_reliable = (flag == FLAG_RELIABLE);
            
            // Parse packet based on type
            if (is_reliable) {
                if (bytes_received < 14) continue;
                parse_reliable_packet(buffer.data(), bytes_received, client_addr);
            } else if (flag == FLAG_UNRELIABLE) {
                if (bytes_received < 10) continue;
                parse_unreliable_packet(buffer.data(), bytes_received, client_addr);
            }
        }
    }
    
    void parse_reliable_packet(const uint8_t* buffer, size_t size, const sockaddr_in& addr) {
        // Reliable packet format: [FLAG][SEQ][PLAYER_ID][MATCH_ID][MSG_TYPE][PAYLOAD]
        const uint8_t* ptr = buffer;
        ptr++; // Skip flag
        
        uint32_t sequence_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
        ptr += 4;
        
        uint32_t player_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
        ptr += 4;
        
        uint32_t match_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
        ptr += 4;
        
        uint8_t msg_type = *ptr++;
        
        // Create game message
        GameMessage* msg = message_pool.acquire();
        if (!msg) return;
        
        pool_allocations.fetch_add(1);
        msg->reset();
        
        msg->player_id = player_id;
        msg->match_id = match_id;
        msg->msg_type = msg_type;
        msg->client_addr = addr;
        msg->payload_size = size - 14;
        
        if (msg->payload_size > 0) {
            memcpy(msg->payload.data(), ptr, (std::min)(msg->payload_size, msg->payload.size()));
        }
        
        enqueue_message(msg);
    }
    
    void parse_unreliable_packet(const uint8_t* buffer, size_t size, const sockaddr_in& addr) {
        // Unreliable packet format: [FLAG][PLAYER_ID][MATCH_ID][MSG_TYPE][PAYLOAD]
        const uint8_t* ptr = buffer;
        ptr++; // Skip flag
        
        uint32_t player_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
        ptr += 4;
        
        uint32_t match_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
        ptr += 4;
        
        uint8_t msg_type = *ptr++;
        
        // Create game message
        GameMessage* msg = message_pool.acquire();
        if (!msg) return;
        
        pool_allocations.fetch_add(1);
        msg->reset();
        
        msg->player_id = player_id;
        msg->match_id = match_id;
        msg->msg_type = msg_type;
        msg->client_addr = addr;
        msg->payload_size = size - 10;
        
        if (msg->payload_size > 0) {
            memcpy(msg->payload.data(), ptr, (std::min)(msg->payload_size, msg->payload.size()));
        }
        
        enqueue_message(msg);
    }
    
    void enqueue_message(GameMessage* msg) {
        static std::atomic<int> thread_index(0);
        int idx = thread_index.fetch_add(1) % NETWORK_THREAD_COUNT;
        
        incoming_queues[idx]->enqueue(incoming_producer_tokens[idx], msg);
        messages_processed.fetch_add(1);
    }
    
    void network_thread(int thread_id) {        
        std::array<GameMessage*, 256> incoming_batch;
        
        while (running.load()) {
            size_t dequeued = incoming_queues[thread_id]->try_dequeue_bulk(
                incoming_consumer_tokens[thread_id], 
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
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
    
    void game_logic_thread(int thread_id) {
        const auto update_interval = std::chrono::milliseconds(33); // ~30 FPS
        
        while (running.load()) {
            for(size_t shard = thread_id; shard < MATCH_SHARD_COUNT; shard += GAME_LOGIC_THREAD_COUNT) {
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
        const auto broadcast_interval = std::chrono::milliseconds(33); // ~30 FPS
        std::array<uint8_t, BUFFER_SIZE> buffer;
        
        while (running.load()) {
            for(size_t shard = thread_id; shard < MATCH_SHARD_COUNT; shard += BROADCAST_THREAD_COUNT) {
                for (auto it = match_shards[shard].begin(); it != match_shards[shard].end(); it++) {
                    auto now = std::chrono::steady_clock::now();
                    
                    if (now - it->second->last_broadcast >= broadcast_interval) {
                        it->second->last_broadcast = now;
                        
                        uint8_t* ptr = buffer.data();
                        size_t total_size;
                        it->second->SerializeGameState(ptr, total_size);
                        
                        // Broadcast game state using UNRELIABLE channel (high frequency)
                        for(size_t player = 0; player < it->second->GetAllPlayer().size(); player++) {
                            auto p = players.find(it->second->GetAllPlayer()[player]->GetId());
                            if (p != players.end()) {
                                dual_channel->send_unreliable(
                                    p->second->id,
                                    it->first,
                                    MSG_GAME_STATE,
                                    ptr,
                                    total_size,
                                    p->second->addr
                                );
                                messages_sent.fetch_add(1);
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
            case MSG_PLAYER_JOIN: // Reliable
                handle_player_join(msg);
                break;
            case MSG_PLAYER_LEAVE: // Reliable
                handle_player_disconnect(msg);
                break;
            case MSG_POSITION: // Unreliable
                handle_player_movement(msg);
                break;
            default:
                handle_player_action(msg);
                break;
        }
    }
    
    void handle_player_join(const GameMessage& msg) {
        auto it = players.find(msg.player_id);
        if (it == players.end()) {
            auto player = std::make_unique<Player>();
            player->id = msg.player_id;
            player->addr = msg.client_addr;
            player->last_seen = std::chrono::steady_clock::now();
            player->active = true;
            
            players[msg.player_id] = std::move(player);
            active_connections.fetch_add(1);
            
            // Send join confirmation via RELIABLE channel
            uint8_t response[4];
            *reinterpret_cast<uint32_t*>(response) = htonl(msg.player_id);
            
            dual_channel->send_reliable(
                msg.player_id,
                0,
                MSG_PLAYER_JOIN,
                response,
                4,
                msg.client_addr
            );
            
            std::cout << "[JOIN] Player " << msg.player_id << " connected\n";
        }
    }
    
    void handle_player_movement(const GameMessage& msg) {
        auto it = players.find(msg.player_id);
        if (it != players.end() && msg.payload_size >= sizeof(float) * 3) {
            it->second->last_seen = std::chrono::steady_clock::now();
            // Movement updates are already unreliable, no need to send confirmation
        }
    }
    
    void handle_player_action(const GameMessage& msg) {
        auto it = players.find(msg.player_id);
        if (it != players.end()) {
            it->second->last_seen = std::chrono::steady_clock::now();
            // Process action...
        }
    }
    
    void handle_player_disconnect(const GameMessage& msg) {
        auto it = players.find(msg.player_id);
        if (it != players.end()) {
            std::cout << "[LEAVE] Player " << msg.player_id << " disconnected\n";
            players.erase(it);
            active_connections.fetch_sub(1);
        }
    }
    
    void monitor_performance() {
        auto last_check = std::chrono::steady_clock::now();
        uint64_t last_messages_processed = 0;
        uint64_t last_messages_sent = 0;
        
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_check).count();
            
            uint64_t current_processed = messages_processed.load();
            uint64_t current_sent = messages_sent.load();
            
            if (elapsed > 0) {
                uint64_t processed_rate = (current_processed - last_messages_processed) / elapsed;
                uint64_t sent_rate = (current_sent - last_messages_sent) / elapsed;
                
                std::cout << "\n========== SERVER PERFORMANCE ==========\n";
                std::cout << "[PLAYERS]\n"
                         << "  Active: " << active_connections.load() << "\n";
                std::cout << "[MESSAGES]\n"
                         << "  In/sec:  " << processed_rate << "\n"
                         << "  Out/sec: " << sent_rate << "\n"
                         << "  Total In:  " << current_processed << "\n"
                         << "  Total Out: " << current_sent << "\n";
                std::cout << "[MEMORY POOL]\n"
                         << "  Active:   " << message_pool.get_allocated_count() << "\n"
                         << "  Capacity: " << message_pool.get_total_capacity() << "\n";
                
                // Print dual-channel statistics
                if (dual_channel) {
                    dual_channel->print_stats();
                }
                
                std::cout << "========================================\n\n";
            }
            
            last_check = now;
            last_messages_processed = current_processed;
            last_messages_sent = current_sent;
        }
    }
};