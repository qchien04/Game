#pragma once

#include "common.h"
#include "MemoryPool.h"
#include "concurrentqueue.h"
#include <mutex>

class TCPConnectionManager {
private:
    SOCKET server_socket;
    HANDLE iocp_handle;
    std::atomic<bool> running;
    
    // Thread management
    std::vector<std::thread> worker_threads;
    std::vector<std::thread> logic_threads;
    
    // Connection management
    std::unordered_map<SOCKET, std::unique_ptr<TCPConnection>> connections;
    std::unordered_map<uint32_t, std::unique_ptr<User>> users;
    std::unordered_map<uint32_t, std::unique_ptr<GameRoom>> rooms;
    std::unordered_map<std::string, uint32_t> username_to_userid;
    std::mutex connections_mutex;
    
    // Concurrent queues for message processing
    using MessageQueue = moodycamel::ConcurrentQueue<std::pair<SOCKET, ProtocolMessage>>;
    std::vector<std::unique_ptr<MessageQueue>> message_queues;
    std::vector<moodycamel::ProducerToken> producer_tokens;
    std::vector<moodycamel::ConsumerToken> consumer_tokens;
    
    // Memory pool for messages
    MemoryPool<ProtocolMessage> message_pool;
    
    // Atomic counters
    std::atomic<uint32_t> next_user_id;
    std::atomic<uint32_t> next_room_id;
    std::atomic<uint64_t> total_connections;
    std::atomic<uint64_t> active_connections;
    std::atomic<uint64_t> messages_processed;
    std::atomic<uint64_t> messages_sent;
    
    // Configuration
    static constexpr int WORKER_THREAD_COUNT = 4;
    static constexpr int LOGIC_THREAD_COUNT = 1;
    static constexpr int MAX_CONNECTIONS = 10000;
    static constexpr int HEARTBEAT_TIMEOUT = 30;
    static constexpr size_t READ_BUFFER_SIZE = 8192;
    static constexpr size_t WRITE_BUFFER_SIZE = 8192;

    // IOCP operation types
    enum class IOOperation {
        ACCEPT,
        RECV,
        SEND
    };

    // Per-operation data
    struct IOContext {
        OVERLAPPED overlapped;
        IOOperation operation;
        SOCKET socket;
        WSABUF wsa_buf;
        std::vector<uint8_t> buffer;
        
        IOContext(IOOperation op, SOCKET s) : operation(op), socket(s) {
            ZeroMemory(&overlapped, sizeof(overlapped));
            buffer.resize(READ_BUFFER_SIZE);
            wsa_buf.buf = reinterpret_cast<char*>(buffer.data());
            wsa_buf.len = static_cast<ULONG>(buffer.size());
        }
    };
    
public:
    TCPConnectionManager() 
        : server_socket(INVALID_SOCKET), iocp_handle(NULL), running(false),
          next_user_id(1), next_room_id(1), total_connections(0),
          active_connections(0), messages_processed(0), messages_sent(0) {
        
        // Initialize Winsock
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        
        // Initialize message queues
        for (int i = 0; i < WORKER_THREAD_COUNT; ++i) {
            message_queues.push_back(std::make_unique<MessageQueue>(8192));
            producer_tokens.emplace_back(*message_queues[i]);
            consumer_tokens.emplace_back(*message_queues[i]);
        }
    }
    
    void add_test() {
        auto user = std::make_unique<User>();
        user->user_id = next_user_id.fetch_add(1);
        user->username = "admin";
        user->password_hash = "123";
        user->room_id = 0;
        user->state = ConnectionState::AUTHENTICATED;
        user->last_heartbeat = std::chrono::steady_clock::now();
        
        uint32_t user_id = user->user_id;
        users[user_id] = std::move(user);
        username_to_userid["admin"] = user_id;

        auto user2 = std::make_unique<User>();
        user2->user_id = next_user_id.fetch_add(1);
        user2->username = "admin2";
        user2->password_hash = "123";
        user2->room_id = 0;
        user2->state = ConnectionState::AUTHENTICATED;
        user2->last_heartbeat = std::chrono::steady_clock::now();
        
        uint32_t user2_id = user2->user_id;
        users[user2_id] = std::move(user2);
        username_to_userid["admin2"] = user2_id;
    }

    bool initialize(int port) {
        add_test();

        // Create IOCP handle
        iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (iocp_handle == NULL) {
            std::cerr << "Failed to create IOCP: " << GetLastError() << std::endl;
            return false;
        }

        // Create TCP socket
        server_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 
                                  NULL, 0, WSA_FLAG_OVERLAPPED);
        if (server_socket == INVALID_SOCKET) {
            std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        // Set socket options
        BOOL opt = TRUE;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
                  (char*)&opt, sizeof(opt));
        
        // Increase buffer sizes
        int buffer_size = 1024 * 1024;
        setsockopt(server_socket, SOL_SOCKET, SO_RCVBUF, 
                  (char*)&buffer_size, sizeof(buffer_size));
        setsockopt(server_socket, SOL_SOCKET, SO_SNDBUF, 
                  (char*)&buffer_size, sizeof(buffer_size));
        
        // Bind socket
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to bind: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            return false;
        }
        
        // Listen
        if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Failed to listen: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            return false;
        }
        
        std::cout << "TCP Server initialized on port " << port << std::endl;
        return true;
    }
    
    void start() {
        running.store(true);
        
        // Start worker threads (IOCP workers)
        for (int i = 0; i < WORKER_THREAD_COUNT; ++i) {
            worker_threads.emplace_back(&TCPConnectionManager::iocp_worker_thread, this, i);
        }
        
        // Start message processing threads
        for (int i = 0; i < WORKER_THREAD_COUNT; ++i) {
            worker_threads.emplace_back(&TCPConnectionManager::message_worker_thread, this, i);
        }
        
        // Start logic threads
        for (int i = 0; i < LOGIC_THREAD_COUNT; ++i) {
            logic_threads.emplace_back(&TCPConnectionManager::logic_thread, this, i);
        }
        
        // Start monitoring thread
        std::thread monitor_thread(&TCPConnectionManager::monitor_thread, this);
        
        std::cout << "Server started with " << WORKER_THREAD_COUNT 
                  << " IOCP workers" << std::endl;
        
        // Accept loop
        accept_loop();
        
        // Wait for threads
        for (auto& t : worker_threads) t.join();
        for (auto& t : logic_threads) t.join();
        monitor_thread.join();
    }
    
    void stop() {
        running.store(false);
        
        if (iocp_handle != NULL) {
            // Post quit messages to worker threads
            for (int i = 0; i < WORKER_THREAD_COUNT * 2; ++i) {
                PostQueuedCompletionStatus(iocp_handle, 0, 0, NULL);
            }
        }
    }
    
    ~TCPConnectionManager() {
        stop();
        if (server_socket != INVALID_SOCKET) {
            closesocket(server_socket);
        }
        if (iocp_handle != NULL) {
            CloseHandle(iocp_handle);
        }
        WSACleanup();
    }

private:
    void accept_loop() {
        while (running.load()) {
            sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            
            SOCKET client_socket = accept(server_socket, 
                                         (sockaddr*)&client_addr, &addr_len);
            
            if (client_socket == INVALID_SOCKET) {
                if (WSAGetLastError() != WSAEWOULDBLOCK) {
                    std::cerr << "Accept error: " << WSAGetLastError() << std::endl;
                }
                continue;
            }
            
            if (active_connections.load() >= MAX_CONNECTIONS) {
                closesocket(client_socket);
                continue;
            }
            
            // Create connection
            auto conn = std::make_unique<TCPConnection>(client_socket, client_addr);
            
            // Associate with IOCP
            if (CreateIoCompletionPort((HANDLE)client_socket, iocp_handle, 
                                      (ULONG_PTR)client_socket, 0) == NULL) {
                std::cerr << "Failed to associate socket with IOCP" << std::endl;
                closesocket(client_socket);
                continue;
            }
            
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                connections[client_socket] = std::move(conn);
            }
            
            total_connections.fetch_add(1);
            active_connections.fetch_add(1);
            
            // Start async receive
            post_receive(client_socket);
            
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
            std::cout << "New connection: " << ip << ":" 
                     << ntohs(client_addr.sin_port) << std::endl;
        }
    }
    
    void post_receive(SOCKET socket) {
        auto io_ctx = new IOContext(IOOperation::RECV, socket);
        
        DWORD flags = 0;
        DWORD bytes_received = 0;
        
        int result = WSARecv(socket, &io_ctx->wsa_buf, 1, &bytes_received,
                            &flags, &io_ctx->overlapped, NULL);
        
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                delete io_ctx;
                close_connection(socket);
            }
        }
    }
    
    void post_send(SOCKET socket, const std::vector<uint8_t>& data) {
        auto io_ctx = new IOContext(IOOperation::SEND, socket);
        io_ctx->buffer = data;
        io_ctx->wsa_buf.buf = reinterpret_cast<char*>(io_ctx->buffer.data());
        io_ctx->wsa_buf.len = static_cast<ULONG>(io_ctx->buffer.size());
        
        DWORD bytes_sent = 0;
        
        int result = WSASend(socket, &io_ctx->wsa_buf, 1, &bytes_sent,
                            0, &io_ctx->overlapped, NULL);
        
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                delete io_ctx;
                close_connection(socket);
            }
        }
    }
    
    void iocp_worker_thread(int thread_id) {
        std::cout<<thread_id<<std::endl;
        while (running.load()) {
            DWORD bytes_transferred = 0;
            ULONG_PTR completion_key = 0;
            LPOVERLAPPED overlapped = NULL;
            
            BOOL result = GetQueuedCompletionStatus(iocp_handle, &bytes_transferred,
                                                   &completion_key, &overlapped, 1000);
            
            if (!result && overlapped == NULL) {
                // Timeout or error
                continue;
            }
            
            if (overlapped == NULL) {
                // Shutdown signal
                break;
            }
            
            auto io_ctx = CONTAINING_RECORD(overlapped, IOContext, overlapped);
            SOCKET socket = io_ctx->socket;
            
            if (bytes_transferred == 0 && io_ctx->operation == IOOperation::RECV) {
                // Connection closed
                delete io_ctx;
                close_connection(socket);
                continue;
            }
            
            switch (io_ctx->operation) {
                case IOOperation::RECV:
                    handle_receive(socket, io_ctx->buffer.data(), bytes_transferred);
                    delete io_ctx;
                    post_receive(socket); // Continue receiving
                    break;
                    
                case IOOperation::SEND:
                    messages_sent.fetch_add(1);
                    delete io_ctx;
                    break;
                    
                default:
                    delete io_ctx;
                    break;
            }
        }
    }
    
    void handle_receive(SOCKET socket, uint8_t* data, DWORD bytes) {
        std::lock_guard<std::mutex> lock(connections_mutex);
        auto it = connections.find(socket);
        if (it == connections.end()) return;
        
        auto& conn = it->second;
        
        // Append to read buffer
        conn->read_buffer.insert(conn->read_buffer.end(), data, data + bytes);
        conn->bytes_read += bytes;
        conn->last_activity = std::chrono::steady_clock::now();
        
        // Process messages
        process_received_data(conn.get());
    }
    
    void process_received_data(TCPConnection* conn) {
        while (conn->bytes_read > 0) {
            if (!conn->header_complete) {
                if (conn->bytes_read < 8) break;
                
                conn->expected_message_length = ntohl(
                    *reinterpret_cast<uint32_t*>(conn->read_buffer.data()));
                
                if (conn->expected_message_length > 1024 * 1024) {
                    close_connection(conn->socket_fd);
                    return;
                }
                
                conn->header_complete = true;
            }
            
            if (conn->bytes_read < conn->expected_message_length) break;
            
            ProtocolMessage msg;
            if (msg.deserialize(conn->read_buffer.data(), 
                              conn->expected_message_length)) {
                static std::atomic<int> worker_index(0);
                int idx = worker_index.fetch_add(1) % WORKER_THREAD_COUNT;
                
                message_queues[idx]->enqueue(producer_tokens[idx], 
                                           std::make_pair(conn->socket_fd, std::move(msg)));
                messages_processed.fetch_add(1);
            }
            
            size_t remaining = conn->bytes_read - conn->expected_message_length;
            if (remaining > 0) {
                memmove(conn->read_buffer.data(), 
                       conn->read_buffer.data() + conn->expected_message_length, 
                       remaining);
            }
            conn->read_buffer.resize(remaining);
            conn->bytes_read = remaining;
            conn->header_complete = false;
            conn->expected_message_length = 0;
        }
    }
    
    void process_message(int client_fd, const ProtocolMessage& msg) {
        auto it = connections.find(client_fd);
        if (it == connections.end()) return;
        
        auto& conn = it->second;
        
        switch (static_cast<MessageType>(msg.type)) {
            case MessageType::LOGIN_REQUEST:
                handle_login_request(conn.get(), msg);
                break;
            case MessageType::REGISTER_REQUEST:
                handle_register_request(conn.get(), msg);
                break;
            case MessageType::LOGOUT_REQUEST:
                handle_logout_request(conn.get(), msg);
                break;
            case MessageType::CREATE_ROOM_REQUEST:
                handle_create_room_request(conn.get(), msg);
                break;
            case MessageType::JOIN_ROOM_REQUEST:
                handle_join_room_request(conn.get(), msg);
                break;
            case MessageType::LEAVE_ROOM_REQUEST:
                handle_leave_room_request(conn.get(), msg);
                break;
            case MessageType::LIST_ROOMS_REQUEST:
                handle_list_rooms_request(conn.get(), msg);
                break;
            case MessageType::START_GAME_REQUEST:
                handle_start_game_request(conn.get(), msg);
                break;
            case MessageType::GAME_READY_REQUEST:
                handle_game_ready_request(conn.get(), msg);
                break;
            case MessageType::HEARTBEAT:
                handle_heartbeat(conn.get(), msg);
                break;
            default:
                send_error_response(conn.get(), msg.sequence, "Unknown message type");
                break;
        }
    }
    
    void message_worker_thread(int thread_id) {
        std::array<std::pair<SOCKET, ProtocolMessage>, 256> batch;
        
        while (running.load()) {
            size_t dequeued = message_queues[thread_id]->try_dequeue_bulk(
                consumer_tokens[thread_id], batch.begin(), batch.size());
            
            if (dequeued > 0) {
                for (size_t i = 0; i < dequeued; ++i) {
                    process_message(batch[i].first, batch[i].second);
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
    
    void logic_thread(int thread_id) {
        std::cout<<thread_id<<std::endl;
        auto last_cleanup = std::chrono::steady_clock::now();
        const auto cleanup_interval = std::chrono::seconds(30);
        
        while (running.load()) {
            auto now = std::chrono::steady_clock::now();
            
            if (now - last_cleanup >= cleanup_interval) {
                cleanup_inactive_connections();
                cleanup_empty_rooms();
                last_cleanup = now;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    void close_connection(SOCKET socket) {
        std::lock_guard<std::mutex> lock(connections_mutex);
        auto it = connections.find(socket);
        if (it != connections.end()) {
            if (it->second->user) {
                handle_user_logout(it->second->user->user_id);
            }
            
            closesocket(socket);
            connections.erase(it);
            active_connections.fetch_sub(1);
        }
    }
    
    void send_message(TCPConnection* conn, const ProtocolMessage& msg) {
        std::vector<uint8_t> buffer;
        msg.serialize(buffer);
        post_send(conn->socket_fd, buffer);
    }
    
    void monitor_thread() {
        auto last_check = std::chrono::steady_clock::now();
        uint64_t last_processed = 0;
        uint64_t last_sent = 0;
        
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(100));
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_check).count();
            
            if (elapsed > 0) {
                uint64_t current_processed = messages_processed.load();
                uint64_t current_sent = messages_sent.load();
                
                std::cout << "Server Stats:\n"
                         << "  Active Connections: " << active_connections.load() << "\n"
                         << "  Total Connections: " << total_connections.load() << "\n"
                         << "  Messages/sec (In): " 
                         << (current_processed - last_processed) / elapsed << "\n"
                         << "  Messages/sec (Out): " 
                         << (current_sent - last_sent) / elapsed << std::endl;
                
                last_processed = current_processed;
                last_sent = current_sent;
            }
            
            last_check = now;
        }
    }
    
    void handle_login_request(TCPConnection* conn, const ProtocolMessage& msg) {
        if (msg.payload.size() < 8) {
            send_error_response(conn, msg.sequence, "Invalid login request");
            return;
        }
        
        // Parse username and password (simplified)
        const char* payload = reinterpret_cast<const char*>(msg.payload.data());
        uint32_t username_len = ntohl(*reinterpret_cast<const uint32_t*>(payload));
        payload += sizeof(uint32_t);
        
        if (username_len > 255 || msg.payload.size() < 8 + username_len) {
            send_error_response(conn, msg.sequence, "Invalid username length");
            return;
        }
        
        std::string username(payload, username_len);
        payload += username_len;
        
        uint32_t password_len = ntohl(*reinterpret_cast<const uint32_t*>(payload));
        payload += sizeof(uint32_t);
        
        if (password_len > 255 || msg.payload.size() < 8 + username_len + password_len) {
            send_error_response(conn, msg.sequence, "Invalid password length");
            return;
        }
        
        std::string password(payload, password_len);
        
        // Authenticate user (simplified - in production, use proper hashing)
        auto user_it = username_to_userid.find(username);

        if (user_it == username_to_userid.end()) {
            send_login_response(conn, msg.sequence, false, 0, "User not found");
            return;
        }
        
        auto& user = users[user_it->second];
        if (user->password_hash != password) {
            send_login_response(conn, msg.sequence, false, 0, "Invalid password");
            return;
        }
        
        // Login successful
        conn->user = std::make_unique<User>(*user);
        conn->state = ConnectionState::AUTHENTICATED;
        
        send_login_response(conn, msg.sequence, true, user->user_id, "Login successful");
        
        std::cout << "User " << username << " logged in successfully" << std::endl;
    }
    
    void handle_register_request(TCPConnection* conn, const ProtocolMessage& msg) {
        // Similar to login but create new user
        if (msg.payload.size() < 8) {
            send_error_response(conn, msg.sequence, "Invalid register request");
            return;
        }
        
        const char* payload = reinterpret_cast<const char*>(msg.payload.data());
        uint32_t username_len = ntohl(*reinterpret_cast<const uint32_t*>(payload));
        payload += sizeof(uint32_t);
        
        if (username_len > 255 || msg.payload.size() < 8 + username_len) {
            send_error_response(conn, msg.sequence, "Invalid username length");
            return;
        }
        
        std::string username(payload, username_len);
        payload += username_len;
        
        uint32_t password_len = ntohl(*reinterpret_cast<const uint32_t*>(payload));
        payload += sizeof(uint32_t);
        
        if (password_len > 255 || msg.payload.size() < 8 + username_len + password_len) {
            send_error_response(conn, msg.sequence, "Invalid password length");
            return;
        }
        
        std::string password(payload, password_len);
        
        // Check if username already exists
        if (username_to_userid.find(username) != username_to_userid.end()) {
            send_register_response(conn, msg.sequence, false, 0, "Username already exists");
            return;
        }
        
        // Create new user
        auto user = std::make_unique<User>();
        user->user_id = next_user_id.fetch_add(1);
        user->username = username;
        user->password_hash = password; // In production, use proper hashing
        user->room_id = 0;
        user->state = ConnectionState::AUTHENTICATED;
        user->last_heartbeat = std::chrono::steady_clock::now();
        
        uint32_t user_id = user->user_id;
        users[user_id] = std::move(user);
        username_to_userid[username] = user_id;
        
        // Set connection user
        conn->user = std::make_unique<User>(*users[user_id]);
        conn->state = ConnectionState::AUTHENTICATED;
        
        send_register_response(conn, msg.sequence, true, user_id, "Registration successful");
        
        std::cout << "User " << username << " registered successfully with ID " << user_id << std::endl;
    }
    
    void handle_logout_request(TCPConnection* conn, const ProtocolMessage& msg) {
        if (!conn->user) {
            send_error_response(conn, msg.sequence, "Not authenticated");
            return;
        }
        
        handle_user_logout(conn->user->user_id);
        send_logout_response(conn, msg.sequence, true, "Logout successful");
        
        std::cout << "User " << conn->user->username << " logged out" << std::endl;
    }
    
    void handle_create_room_request(TCPConnection* conn, const ProtocolMessage& msg) {
        if (!conn->user || conn->state != ConnectionState::AUTHENTICATED) {
            send_error_response(conn, msg.sequence, "Not authenticated");
            return;
        }
        
        if (conn->user->room_id != 0) {
            send_error_response(conn, msg.sequence, "Already in a room");
            return;
        }
        
        if (msg.payload.size() < 8) {
            send_error_response(conn, msg.sequence, "Invalid create room request");
            return;
        }
        
        const char* payload = reinterpret_cast<const char*>(msg.payload.data());
        uint32_t room_name_len = ntohl(*reinterpret_cast<const uint32_t*>(payload));
        payload += sizeof(uint32_t);
        
        if (room_name_len > 255 || msg.payload.size() < 8 + room_name_len) {
            send_error_response(conn, msg.sequence, "Invalid room name length");
            return;
        }
        
        std::string room_name(payload, room_name_len);
        payload += room_name_len;
        
        uint32_t max_players = ntohl(*reinterpret_cast<const uint32_t*>(payload));
        
        if (max_players < 2 || max_players > 16) {
            send_error_response(conn, msg.sequence, "Invalid max players count");
            return;
        }
        
        // Create new room
        auto room = std::make_unique<GameRoom>();
        room->room_id = next_room_id.fetch_add(1);
        room->room_name = room_name;
        room->max_players = max_players;
        room->current_players = 1;
        room->owner_id = conn->user->user_id;
        room->state = RoomState::WAITING;
        room->player_list.push_back(conn->user->user_id);
        room->created_time = std::chrono::steady_clock::now();
        
        uint32_t room_id = room->room_id;
        rooms[room_id] = std::move(room);
        
        // Update user and connection state
        conn->user->room_id = room_id;
        conn->state = ConnectionState::IN_ROOM;
        users[conn->user->user_id]->room_id = room_id;
        
        send_create_room_response(conn, msg.sequence, true, room_id, "Room created successfully");
        
        std::cout << "Room " << room_name << " created by user " << conn->user->username 
                  << " with ID " << room_id << std::endl;
    }
    
    void handle_join_room_request(TCPConnection* conn, const ProtocolMessage& msg) {
        if (!conn->user || conn->state != ConnectionState::AUTHENTICATED) {
            send_error_response(conn, msg.sequence, "Not authenticated");
            return;
        }
        
        if (conn->user->room_id != 0) {
            send_error_response(conn, msg.sequence, "Already in a room");
            return;
        }
        
        if (msg.payload.size() < 4) {
            send_error_response(conn, msg.sequence, "Invalid join room request");
            return;
        }
        
        uint32_t room_id = ntohl(*reinterpret_cast<const uint32_t*>(msg.payload.data()));
        
        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) {
            send_join_room_response(conn, msg.sequence, false, 0, "Room not found");
            return;
        }
        
        auto& room = room_it->second;
        
        if (room->current_players >= room->max_players) {
            send_join_room_response(conn, msg.sequence, false, 0, "Room is full");
            return;
        }
        
        if (room->state != RoomState::WAITING) {
            send_join_room_response(conn, msg.sequence, false, 0, "Room is not accepting players");
            return;
        }
        
        // Add player to room
        room->player_list.push_back(conn->user->user_id);
        room->current_players++;
        
        // Update user and connection state
        conn->user->room_id = room_id;
        conn->state = ConnectionState::IN_ROOM;
        users[conn->user->user_id]->room_id = room_id;
        
        send_join_room_response(conn, msg.sequence, true, room_id, "Joined room successfully");
        
        // Notify all players in room about new player
        broadcast_room_state_update(room_id);
        
        std::cout << "User " << conn->user->username << " joined room " << room_id << std::endl;
    }
    
    void handle_leave_room_request(TCPConnection* conn, const ProtocolMessage& msg) {
        if (!conn->user || conn->state != ConnectionState::IN_ROOM) {
            send_error_response(conn, msg.sequence, "Not in a room");
            return;
        }
        
        uint32_t room_id = conn->user->room_id;
        remove_player_from_room(conn->user->user_id, room_id);
        
        send_leave_room_response(conn, msg.sequence, true, "Left room successfully");
        
        std::cout << "User " << conn->user->username << " left room " << room_id << std::endl;
    }
    
    void handle_list_rooms_request(TCPConnection* conn, const ProtocolMessage& msg) {
        if (!conn->user || conn->state != ConnectionState::AUTHENTICATED) {
            send_error_response(conn, msg.sequence, "Not authenticated");
            return;
        }
        send_list_rooms_response(conn, msg.sequence);
    }
    
    void handle_start_game_request(TCPConnection* conn, const ProtocolMessage& msg) {
        if (!conn->user || conn->state != ConnectionState::IN_ROOM) {
            send_error_response(conn, msg.sequence, "Not in a room");
            return;
        }
        
        uint32_t room_id = conn->user->room_id;
        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) {
            send_error_response(conn, msg.sequence, "Room not found");
            return;
        }
        
        auto& room = room_it->second;
        
        if (room->owner_id != conn->user->user_id) {
            send_error_response(conn, msg.sequence, "Only room owner can start game");
            return;
        }
        
        if (room->current_players < 1) {
            send_error_response(conn, msg.sequence, "Need at least 2 players to start");
            return;
        }
        
        if (room->state != RoomState::WAITING) {
            send_error_response(conn, msg.sequence, "Game already in progress");
            return;
        }
        
        // Start game
        room->state = RoomState::STARTING;
        

        // Send to all players 

            std::cout<<"[CREATE] new match" <<std::endl;

        
        
        // Notify all players about game start
        broadcast_game_start_notification(room_id,1);
        
        send_start_game_response(conn, msg.sequence, true, "Game starting");
        
        std::cout << "Game started in room " << room_id << " by " << conn->user->username << std::endl;
    }
    
    void handle_game_ready_request(TCPConnection* conn, const ProtocolMessage& msg) {
        if (!conn->user || conn->state != ConnectionState::IN_ROOM) {
            send_error_response(conn, msg.sequence, "Not in a room");
            return;
        }
        
        // Update connection state to in-game
        conn->state = ConnectionState::IN_GAME;
        users[conn->user->user_id]->state = ConnectionState::IN_GAME;
        
        send_game_ready_response(conn, msg.sequence, true, "Ready for game");
        
        std::cout << "User " << conn->user->username << " is ready for game" << std::endl;
    }
    
    void handle_heartbeat(TCPConnection* conn, const ProtocolMessage& msg) {
        conn->last_activity = std::chrono::steady_clock::now();
        if (conn->user) {
            conn->user->last_heartbeat = conn->last_activity;
            users[conn->user->user_id]->last_heartbeat = conn->last_activity;
        }
        
        // Send heartbeat response
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::HEARTBEAT);
        response.sequence = msg.sequence;
        response.length = 8;
        
        send_message(conn, response);
    }
    
    // Response sending functions
    void send_login_response(TCPConnection* conn, uint16_t sequence, bool success, int32_t user_id, const std::string& message) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::LOGIN_RESPONSE);
        response.sequence = sequence;
        
        // Payload: success(1) + user_id(4) + message_len(4) + message
        response.payload.resize(9 + message.length());
        uint8_t* ptr = response.payload.data();
        
        *ptr++ = success ? 1 : 0;
        
        uint32_t net_user_id = htonl(user_id);
        memcpy(ptr, &net_user_id, sizeof(net_user_id));
        ptr += sizeof(net_user_id);
        
        uint32_t msg_len = htonl(message.length());
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        
        memcpy(ptr, message.c_str(), message.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_register_response(TCPConnection* conn, uint16_t sequence, bool success, uint32_t user_id, const std::string& message) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::REGISTER_RESPONSE);
        response.sequence = sequence;
        
        response.payload.resize(9 + message.length());
        uint8_t* ptr = response.payload.data();
        
        *ptr++ = success ? 1 : 0;
        
        uint32_t net_user_id = htonl(user_id);
        memcpy(ptr, &net_user_id, sizeof(net_user_id));
        ptr += sizeof(net_user_id);
        
        uint32_t msg_len = htonl(message.length());
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        
        memcpy(ptr, message.c_str(), message.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_logout_response(TCPConnection* conn, uint16_t sequence, bool success, const std::string& message) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::LOGOUT_RESPONSE);
        response.sequence = sequence;
        
        response.payload.resize(5 + message.length());
        uint8_t* ptr = response.payload.data();
        
        *ptr++ = success ? 1 : 0;
        
        uint32_t msg_len = htonl(message.length());
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        
        memcpy(ptr, message.c_str(), message.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_create_room_response(TCPConnection* conn, uint16_t sequence, bool success, uint32_t room_id, const std::string& message) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::CREATE_ROOM_RESPONSE);
        response.sequence = sequence;
        
        response.payload.resize(9 + message.length());
        uint8_t* ptr = response.payload.data();
        
        *ptr++ = success ? 1 : 0;
        
        uint32_t net_room_id = htonl(room_id);
        memcpy(ptr, &net_room_id, sizeof(net_room_id));
        ptr += sizeof(net_room_id);
        
        uint32_t msg_len = htonl(message.length());
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        
        memcpy(ptr, message.c_str(), message.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_join_room_response(TCPConnection* conn, uint16_t sequence, bool success, uint32_t room_id, const std::string& message) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::JOIN_ROOM_RESPONSE);
        response.sequence = sequence;
        
        response.payload.resize(9 + message.length());
        uint8_t* ptr = response.payload.data();
        
        *ptr++ = success ? 1 : 0;
        
        uint32_t net_room_id = htonl(room_id);
        memcpy(ptr, &net_room_id, sizeof(net_room_id));
        ptr += sizeof(net_room_id);
        
        uint32_t msg_len = htonl(message.length());
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        
        memcpy(ptr, message.c_str(), message.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_leave_room_response(TCPConnection* conn, uint16_t sequence, bool success, const std::string& message) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::LEAVE_ROOM_RESPONSE);
        response.sequence = sequence;
        
        response.payload.resize(5 + message.length());
        uint8_t* ptr = response.payload.data();
        
        *ptr++ = success ? 1 : 0;
        
        uint32_t msg_len = htonl(message.length());
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        
        memcpy(ptr, message.c_str(), message.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_list_rooms_response(TCPConnection* conn, uint16_t sequence) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::LIST_ROOMS_RESPONSE);
        response.sequence = sequence;
        
        // Calculate payload size
        size_t payload_size = 4; // room count
        for (const auto& room_pair : rooms) {
            const auto& room = room_pair.second;
            if (room->state == RoomState::WAITING) {
                payload_size += 4 + 4 + room->room_name.length() + 4 + 4 + 1; // room_id + name_len + name + current + max + state
            }
        }
        
        response.payload.resize(payload_size);
        uint8_t* ptr = response.payload.data();
        
        // Count available rooms
        uint32_t room_count = 0;
        for (const auto& room_pair : rooms) {
            if (room_pair.second->state == RoomState::WAITING) {
                room_count++;
            }
        }
        
        uint32_t net_count = htonl(room_count);
        memcpy(ptr, &net_count, sizeof(net_count));
        ptr += sizeof(net_count);
        
        // Add room info
        for (const auto& room_pair : rooms) {
            const auto& room = room_pair.second;
            if (room->state != RoomState::WAITING) continue;
            
            uint32_t net_room_id = htonl(room->room_id);
            memcpy(ptr, &net_room_id, sizeof(net_room_id));
            ptr += sizeof(net_room_id);
            
            uint32_t name_len = htonl(room->room_name.length());
            memcpy(ptr, &name_len, sizeof(name_len));
            ptr += sizeof(name_len);
            
            memcpy(ptr, room->room_name.c_str(), room->room_name.length());
            ptr += room->room_name.length();
            
            uint32_t net_current = htonl(room->current_players);
            memcpy(ptr, &net_current, sizeof(net_current));
            ptr += sizeof(net_current);
            
            uint32_t net_max = htonl(room->max_players);
            memcpy(ptr, &net_max, sizeof(net_max));
            ptr += sizeof(net_max);
            
            *ptr++ = static_cast<uint8_t>(room->state);
        }
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_start_game_response(TCPConnection* conn, uint16_t sequence, bool success, const std::string& message) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::START_GAME_RESPONSE);
        response.sequence = sequence;
        
        response.payload.resize(5 + message.length());
        uint8_t* ptr = response.payload.data();
        
        *ptr++ = success ? 1 : 0;
        
        uint32_t msg_len = htonl(message.length());
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        
        memcpy(ptr, message.c_str(), message.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_game_ready_response(TCPConnection* conn, uint16_t sequence, bool success, const std::string& message) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::GAME_READY_RESPONSE);
        response.sequence = sequence;
        
        response.payload.resize(5 + message.length());
        uint8_t* ptr = response.payload.data();
        
        *ptr++ = success ? 1 : 0;
        
        uint32_t msg_len = htonl(message.length());
        memcpy(ptr, &msg_len, sizeof(msg_len));
        ptr += sizeof(msg_len);
        
        memcpy(ptr, message.c_str(), message.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    void send_error_response(TCPConnection* conn, uint16_t sequence, const std::string& error) {
        ProtocolMessage response;
        response.type = static_cast<uint16_t>(MessageType::ERROR_RESPONSE);
        response.sequence = sequence;
        
        response.payload.resize(4 + error.length());
        uint8_t* ptr = response.payload.data();
        
        uint32_t error_len = htonl(error.length());
        memcpy(ptr, &error_len, sizeof(error_len));
        ptr += sizeof(error_len);
        
        memcpy(ptr, error.c_str(), error.length());
        
        response.length = 8 + response.payload.size();
        send_message(conn, response);
    }
    
    // Utility functions
    void handle_user_logout(uint32_t user_id) {
        auto user_it = users.find(user_id);
        if (user_it == users.end()) return;
        
        auto& user = user_it->second;
        
        // Remove from room if in one
        if (user->room_id != 0) {
            remove_player_from_room(user_id, user->room_id);
        }
    }
    
    void remove_player_from_room(uint32_t user_id, uint32_t room_id) {
        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) return;
        
        auto& room = room_it->second;
        
        // Remove player from list
        auto player_it = std::find(room->player_list.begin(), room->player_list.end(), user_id);
        if (player_it != room->player_list.end()) {
            room->player_list.erase(player_it);
            room->current_players--;
        }
        
        // Update user state
        auto user_it = users.find(user_id);
        if (user_it != users.end()) {
            user_it->second->room_id = 0;
            user_it->second->state = ConnectionState::AUTHENTICATED;
        }
        
        // If room is empty or owner left, clean up room
        if (room->current_players == 0 || room->owner_id == user_id) {
            rooms.erase(room_it);
        } else {
            // Assign new owner if needed
            if (room->owner_id == user_id && !room->player_list.empty()) {
                room->owner_id = room->player_list[0];
            }
            
            // Notify remaining players
            broadcast_room_state_update(room_id);
        }
    }
    
    void broadcast_room_state_update(uint32_t room_id) {
        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) return;
        
        auto& room = room_it->second;
        
        ProtocolMessage update;
        update.type = static_cast<uint16_t>(MessageType::ROOM_STATE_UPDATE);
        update.sequence = 0;
        
        // Build room state payload
        size_t payload_size = 4 + 4 + room->room_name.length() + 4 + + 4+ 4 + 1 + 4; // basic info + player count
        for (uint32_t player_id : room->player_list) {
            auto user_it = users.find(player_id);
            if (user_it != users.end()) {
                payload_size += 4 + 4 + user_it->second->username.length(); // player_id + name_len + name
            }
        }
        
        update.payload.resize(payload_size);
        uint8_t* ptr = update.payload.data();
        
        // Room ID
        uint32_t net_room_id = htonl(room->room_id);
        memcpy(ptr, &net_room_id, sizeof(net_room_id));
        ptr += sizeof(net_room_id);
        
        // Room name
        uint32_t name_len = htonl(room->room_name.length());
        memcpy(ptr, &name_len, sizeof(name_len));
        ptr += sizeof(name_len);
        memcpy(ptr, room->room_name.c_str(), room->room_name.length());
        ptr += room->room_name.length();
        
        // Player counts
        uint32_t net_current = htonl(room->current_players);
        memcpy(ptr, &net_current, sizeof(net_current));
        ptr += sizeof(net_current);
        
        uint32_t net_max = htonl(room->max_players);
        memcpy(ptr, &net_max, sizeof(net_max));
        ptr += sizeof(net_max);

        uint32_t net_owner = htonl(room->owner_id);
        memcpy(ptr, &net_owner, sizeof(net_owner));
        ptr += sizeof(net_owner);
        
        // Room state
        *ptr++ = static_cast<uint8_t>(room->state);
        
        // Player list
        uint32_t player_count = htonl(room->player_list.size());
        memcpy(ptr, &player_count, sizeof(player_count));
        ptr += sizeof(player_count);
        
        for (uint32_t player_id : room->player_list) {
            auto user_it = users.find(player_id);
            if (user_it != users.end()) {
                uint32_t net_player_id = htonl(player_id);
                memcpy(ptr, &net_player_id, sizeof(net_player_id));
                ptr += sizeof(net_player_id);
                
                uint32_t username_len = htonl(user_it->second->username.length());
                memcpy(ptr, &username_len, sizeof(username_len));
                ptr += sizeof(username_len);
                
                memcpy(ptr, user_it->second->username.c_str(), user_it->second->username.length());
                ptr += user_it->second->username.length();
            }
        }
        
        update.length = 8 + update.payload.size();
        
        // Send to all players in room
        for (uint32_t player_id : room->player_list) {
            // Find connection for this player
            for (const auto& conn_pair : connections) {
                if (conn_pair.second->user && conn_pair.second->user->user_id == player_id) {
                    send_message(conn_pair.second.get(), update);
                    break;
                }
            }
        }
    }
    
    void broadcast_game_start_notification(uint32_t room_id, uint32_t match_id) {
        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) return;
        
        auto& room = room_it->second;
        
        ProtocolMessage notification;
        notification.type = static_cast<uint16_t>(MessageType::START_GAME_REQUEST);
        notification.sequence = 0;
        notification.payload.resize(8);

        uint32_t net_room_id = htonl(room_id);
        uint32_t net_match_id = htonl(match_id);

        memcpy(notification.payload.data(), &net_room_id, sizeof(net_room_id));
        memcpy(notification.payload.data() + 4, &net_match_id, sizeof(net_match_id));
        
        notification.length = 8 + notification.payload.size();
    
        // Send to all players in room
        for (uint32_t player_id : room->player_list) {
            // Find connection for this player
            for (const auto& conn_pair : connections) {
                if (conn_pair.second->user && conn_pair.second->user->user_id == player_id) {
                    send_message(conn_pair.second.get(), notification);
                    break;
                }
            }
        }
    }
    
    void cleanup_inactive_connections() {
        auto now = std::chrono::steady_clock::now();
        std::vector<int> to_close;
        
        for (const auto& conn_pair : connections) {
            const auto& conn = conn_pair.second;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - conn->last_activity).count();
            
            if (elapsed > HEARTBEAT_TIMEOUT) {
                to_close.push_back(conn->socket_fd);
            }
        }
        
        for (int fd : to_close) {
            std::cout << "Closing inactive connection (fd: " << fd << ")" << std::endl;
            close_connection(fd);
        }
    }
    
    void cleanup_empty_rooms() {
        std::vector<uint32_t> to_remove;
        
        for (const auto& room_pair : rooms) {
            if (room_pair.second->current_players == 0) {
                to_remove.push_back(room_pair.first);
            }
        }
        
        for (uint32_t room_id : to_remove) {
            std::cout << "Removing empty room " << room_id << std::endl;
            rooms.erase(room_id);
        }
    }
    
    // Public interface methods
public:
    void broadcast_to_room(uint32_t room_id, const ProtocolMessage& msg) {
        auto room_it = rooms.find(room_id);
        if (room_it == rooms.end()) return;
        
        auto& room = room_it->second;
        
        for (uint32_t player_id : room->player_list) {
            for (const auto& conn_pair : connections) {
                if (conn_pair.second->user && conn_pair.second->user->user_id == player_id) {
                    send_message(conn_pair.second.get(), msg);
                    break;
                }
            }
        }
    }
    
    void broadcast_to_all(const ProtocolMessage& msg) {
        for (const auto& conn_pair : connections) {
            if (conn_pair.second->state == ConnectionState::AUTHENTICATED ||
                conn_pair.second->state == ConnectionState::IN_ROOM ||
                conn_pair.second->state == ConnectionState::IN_GAME) {
                send_message(conn_pair.second.get(), msg);
            }
        }
    }
    
    std::vector<uint32_t> get_room_players(uint32_t room_id) {
        auto room_it = rooms.find(room_id);
        if (room_it != rooms.end()) {
            return room_it->second->player_list;
        }
        return {};
    }
    
    bool is_user_in_room(uint32_t user_id, uint32_t room_id) {
        auto user_it = users.find(user_id);
        return user_it != users.end() && user_it->second->room_id == room_id;
    }
    
    ConnectionState get_user_state(uint32_t user_id) {
        auto user_it = users.find(user_id);
        if (user_it != users.end()) {
            return user_it->second->state;
        }
        return ConnectionState::DISCONNECTED;
    }
    
    //Monitor
    size_t get_active_connections() const {
        return active_connections.load();
    }
    
    size_t get_total_connections() const {
        return total_connections.load();
    }
    
    size_t get_active_users() const {
        return users.size();
    }
    
    size_t get_active_rooms() const {
        return rooms.size();
    }
    
    uint64_t get_messages_processed() const {
        return messages_processed.load();
    }
    
    uint64_t get_messages_sent() const {
        return messages_sent.load();
    }
};
