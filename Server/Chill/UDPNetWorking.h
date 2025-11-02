// DualChannelUDP.h - Giống ENet với 2 kênh riêng biệt
#pragma once
#include <cstdint>
#include <chrono>
#include <stdio.h>
#include <iostream> 
#include <deque>
#include <unordered_map>
#include <mutex>
#include <array>
#include <unordered_set>
#include <winsock2.h>

// Packet header flags
enum class ChannelType : uint8_t {
    UNRELIABLE = 0x01,  // Kênh không cần tin cậy
    RELIABLE = 0x02     // Kênh cần tin cậy
};

// Reliable message với sequence number và timestamp
struct ReliablePacket {
    uint32_t sequence_id;
    uint32_t player_id;
    uint32_t match_id;
    uint32_t msg_type;
    std::chrono::steady_clock::time_point sent_time;
    std::chrono::steady_clock::time_point last_retry_time;
    int retry_count;
    sockaddr_in client_addr;
    std::array<uint8_t, 4096> payload;
    size_t payload_size;
    bool acknowledged;
    
    ReliablePacket() : sequence_id(0), player_id(0), match_id(0), msg_type(0),
                       retry_count(0), payload_size(0), acknowledged(false) {}
};

// Unreliable message - đơn giản, không cần tracking
struct UnreliablePacket {
    uint32_t player_id;
    uint32_t match_id;
    uint32_t msg_type;
    sockaddr_in client_addr;
    std::array<uint8_t, 4096> payload;
    size_t payload_size;
    
    UnreliablePacket() : player_id(0), match_id(0), msg_type(0), payload_size(0) {}
};

// ============ RELIABLE CHANNEL ============
class ReliableChannel {
private:
    std::atomic<uint32_t> next_sequence_id;
    
    // Pending messages chờ ACK (per player)
    std::unordered_map<uint32_t, std::deque<std::unique_ptr<ReliablePacket>>> pending_messages;
    std::mutex pending_mutex;
    
    // Received sequence tracking để detect duplicates
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> received_sequences;
    std::mutex received_mutex;
    
    // Configuration
    static constexpr int MAX_RETRY_COUNT = 5;
    static constexpr int RETRY_TIMEOUT_MS = 100;
    static constexpr int MAX_PENDING_PER_PLAYER = 100;
    
    SOCKET socket;
    std::atomic<uint64_t> packets_sent;
    std::atomic<uint64_t> packets_acked;
    std::atomic<uint64_t> packets_failed;
    std::atomic<uint64_t> packets_retried;
    std::atomic<uint64_t> duplicates_detected;

public:
    ReliableChannel(SOCKET sock) : 
        socket(sock),
        next_sequence_id(1),
        packets_sent(0),
        packets_acked(0),
        packets_failed(0),
        packets_retried(0),
        duplicates_detected(0) {}
    
    // Gửi packet reliable
    bool send(uint32_t player_id, uint32_t match_id, uint32_t msg_type,
              const uint8_t* payload, size_t payload_size,
              const sockaddr_in& client_addr) {
        
        auto packet = std::make_unique<ReliablePacket>();
        packet->sequence_id = next_sequence_id.fetch_add(1);
        packet->player_id = player_id;
        packet->match_id = match_id;
        packet->msg_type = msg_type;
        packet->client_addr = client_addr;
        packet->sent_time = std::chrono::steady_clock::now();
        packet->last_retry_time = packet->sent_time;
        packet->retry_count = 0;
        packet->acknowledged = false;
        
        // Copy payload
        packet->payload_size = (std::min)(payload_size, packet->payload.size());
        if (payload_size > 0) {
            memcpy(packet->payload.data(), payload, packet->payload_size);
        }
        
        // Serialize và gửi
        if (!send_packet(*packet)) {
            return false;
        }
        
        // Add to pending queue
        {
            std::lock_guard<std::mutex> lock(pending_mutex);
            auto& queue = pending_messages[player_id];
            
            if (queue.size() >= MAX_PENDING_PER_PLAYER) {
                std::cerr << "[RELIABLE] Queue full for player " << player_id << std::endl;
                return false;
            }
            
            queue.push_back(std::move(packet));
        }
        
        packets_sent.fetch_add(1);
        return true;
    }
    
    // Xử lý ACK nhận được
    void process_ack(uint32_t player_id, uint32_t sequence_id) {
        std::lock_guard<std::mutex> lock(pending_mutex);
        
        auto it = pending_messages.find(player_id);
        if (it == pending_messages.end()) return;
        
        auto& queue = it->second;
        
        for (auto msg_it = queue.begin(); msg_it != queue.end(); ++msg_it) {
            if ((*msg_it)->sequence_id == sequence_id) {
                queue.erase(msg_it);
                packets_acked.fetch_add(1);
                break;
            }
        }
        
        if (queue.empty()) {
            pending_messages.erase(it);
        }
    }
    
    // Kiểm tra duplicate
    bool is_duplicate(uint32_t player_id, uint32_t sequence_id) {
        std::lock_guard<std::mutex> lock(received_mutex);
        
        auto& sequences = received_sequences[player_id];
        
        if (sequences.find(sequence_id) != sequences.end()) {
            duplicates_detected.fetch_add(1);
            return true;
        }
        
        sequences.insert(sequence_id);
        
        // Giới hạn kích thước
        if (sequences.size() > 1000) {
            uint32_t min_seq = *sequences.begin();
            sequences.erase(min_seq);
        }
        
        return false;
    }
    
    // Retry loop
    void process_retries() {
        auto now = std::chrono::steady_clock::now();
        std::vector<std::unique_ptr<ReliablePacket>> failed_packets;
        
        {
            std::lock_guard<std::mutex> lock(pending_mutex);
            
            for (auto& [player_id, queue] : pending_messages) {
                for (auto msg_it = queue.begin(); msg_it != queue.end(); ) {
                    auto& packet = *msg_it;
                    
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - packet->last_retry_time).count();
                    
                    if (elapsed >= RETRY_TIMEOUT_MS) {
                        if (packet->retry_count < MAX_RETRY_COUNT) {
                            packet->retry_count++;
                            packet->last_retry_time = now;
                            send_packet(*packet);
                            packets_retried.fetch_add(1);
                            ++msg_it;
                        } else {
                            // Failed
                            failed_packets.push_back(std::move(*msg_it));
                            msg_it = queue.erase(msg_it);
                            packets_failed.fetch_add(1);
                        }
                    } else {
                        ++msg_it;
                    }
                }
            }
        }
        
        for (auto& packet : failed_packets) {
            std::cerr << "[RELIABLE] Packet failed: seq=" << packet->sequence_id 
                     << " player=" << packet->player_id << std::endl;
        }
    }
    
    // Serialize và gửi
    bool send_packet(const ReliablePacket& packet) {
        std::array<uint8_t, 8192> buffer;
        uint8_t* ptr = buffer.data();
        
        // Header: [CHANNEL_TYPE][sequence_id][player_id][match_id][msg_type][payload]
        *ptr++ = static_cast<uint8_t>(ChannelType::RELIABLE);
        
        uint32_t net_seq = htonl(packet.sequence_id);
        memcpy(ptr, &net_seq, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        uint32_t net_player = htonl(packet.player_id);
        memcpy(ptr, &net_player, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        uint32_t net_match = htonl(packet.match_id);
        memcpy(ptr, &net_match, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        uint32_t net_type = htonl(packet.msg_type);
        memcpy(ptr, &net_type, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        memcpy(ptr, packet.payload.data(), packet.payload_size);
        ptr += packet.payload_size;
        
        size_t total_size = ptr - buffer.data();
        
        int sent = sendto(socket, (char*)buffer.data(), total_size,
                         0, (sockaddr*)&packet.client_addr, sizeof(packet.client_addr));
        
        return sent > 0;
    }
    
    // Send ACK
    void send_ack(uint32_t player_id, uint32_t sequence_id, const sockaddr_in& client_addr) {
        std::array<uint8_t, 32> buffer;
        uint8_t* ptr = buffer.data();
        
        *ptr++ = 0xFE; // ACK flag
        
        uint32_t net_seq = htonl(sequence_id);
        memcpy(ptr, &net_seq, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        uint32_t net_player = htonl(player_id);
        memcpy(ptr, &net_player, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        size_t total_size = ptr - buffer.data();
        
        sendto(socket, (char*)buffer.data(), total_size,
              0, (sockaddr*)&client_addr, sizeof(client_addr));
    }
    
    void get_stats(uint64_t& sent, uint64_t& acked, uint64_t& failed, 
                   uint64_t& retried, uint64_t& duplicates) {
        sent = packets_sent.load();
        acked = packets_acked.load();
        failed = packets_failed.load();
        retried = packets_retried.load();
        duplicates = duplicates_detected.load();
    }
    
    void cleanup_player(uint32_t player_id) {
        {
            std::lock_guard<std::mutex> lock(pending_mutex);
            pending_messages.erase(player_id);
        }
        {
            std::lock_guard<std::mutex> lock(received_mutex);
            received_sequences.erase(player_id);
        }
    }
};

// ============ UNRELIABLE CHANNEL ============
class UnreliableChannel {
private:
    SOCKET socket;
    std::atomic<uint64_t> packets_sent;
    std::atomic<uint64_t> packets_received;

public:
    UnreliableChannel(SOCKET sock) : 
        socket(sock),
        packets_sent(0),
        packets_received(0) {}
    
    // Gửi packet unreliable - đơn giản, không tracking
    bool send(uint32_t player_id, uint32_t match_id, uint32_t msg_type,
              const uint8_t* payload, size_t payload_size,
              const sockaddr_in& client_addr) {
        
        std::array<uint8_t, 8192> buffer;
        uint8_t* ptr = buffer.data();
        
        // Header: [CHANNEL_TYPE][player_id][match_id][msg_type][payload]
        *ptr++ = static_cast<uint8_t>(ChannelType::UNRELIABLE);
        
        uint32_t net_player = htonl(player_id);
        memcpy(ptr, &net_player, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        uint32_t net_match = htonl(match_id);
        memcpy(ptr, &net_match, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        uint32_t net_type = htonl(msg_type);
        memcpy(ptr, &net_type, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        size_t payload_copy = (std::min)(payload_size, size_t(4096));
        memcpy(ptr, payload, payload_copy);
        ptr += payload_copy;
        
        size_t total_size = ptr - buffer.data();
        
        int sent = sendto(socket, (char*)buffer.data(), total_size,
                         0, (sockaddr*)&client_addr, sizeof(client_addr));
        
        if (sent > 0) {
            packets_sent.fetch_add(1);
            return true;
        }
        return false;
    }
    
    void increment_received() {
        packets_received.fetch_add(1);
    }
    
    void get_stats(uint64_t& sent, uint64_t& received) {
        sent = packets_sent.load();
        received = packets_received.load();
    }
};

// ============ DUAL CHANNEL MANAGER ============
class DualChannelManager {
private:
    std::unique_ptr<ReliableChannel> reliable_channel;
    std::unique_ptr<UnreliableChannel> unreliable_channel;
    std::thread retry_thread;
    std::atomic<bool> running;

public:
    DualChannelManager(SOCKET socket) : running(false) {
        reliable_channel = std::make_unique<ReliableChannel>(socket);
        unreliable_channel = std::make_unique<UnreliableChannel>(socket);
    }
    
    ~DualChannelManager() {
        stop();
    }
    
    void start() {
        running.store(true);
        retry_thread = std::thread([this]() {
            while (running.load()) {
                reliable_channel->process_retries();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    void stop() {
        running.store(false);
        if (retry_thread.joinable()) {
            retry_thread.join();
        }
    }
    
    // Send via RELIABLE channel
    bool send_reliable(uint32_t player_id, uint32_t match_id, uint32_t msg_type,
                      const uint8_t* payload, size_t payload_size,
                      const sockaddr_in& client_addr) {
        return reliable_channel->send(player_id, match_id, msg_type, 
                                     payload, payload_size, client_addr);
    }
    
    // Send via UNRELIABLE channel
    bool send_unreliable(uint32_t player_id, uint32_t match_id, uint32_t msg_type,
                        const uint8_t* payload, size_t payload_size,
                        const sockaddr_in& client_addr) {
        return unreliable_channel->send(player_id, match_id, msg_type,
                                       payload, payload_size, client_addr);
    }
    
    // Process incoming packet
    void process_incoming(const uint8_t* buffer, size_t size, const sockaddr_in& client_addr) {
        if (size < 1) return;
        
        uint8_t channel_type = buffer[0];
        const uint8_t* ptr = buffer + 1;
        
        if (channel_type == static_cast<uint8_t>(ChannelType::RELIABLE)) {
            // Reliable packet
            if (size < 1 + sizeof(uint32_t) * 4) return;
            
            uint32_t sequence_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            uint32_t player_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            // Send ACK immediately
            reliable_channel->send_ack(player_id, sequence_id, client_addr);
            
            // Check duplicate
            if (reliable_channel->is_duplicate(player_id, sequence_id)) {
                return; // Skip duplicate
            }
            
            // Return remaining data for processing
            // (caller will handle match_id, msg_type, payload)
            
        } else if (channel_type == static_cast<uint8_t>(ChannelType::UNRELIABLE)) {
            // Unreliable packet - just count it
            unreliable_channel->increment_received();
            
            // Return data for processing
            
        } else if (channel_type == 0xFE) {
            // ACK packet
            if (size < 1 + sizeof(uint32_t) * 2) return;
            
            uint32_t sequence_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            uint32_t player_id = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
            
            reliable_channel->process_ack(player_id, sequence_id);
        }
    }
    
    void cleanup_player(uint32_t player_id) {
        reliable_channel->cleanup_player(player_id);
    }
    
    void print_stats() {
        uint64_t rel_sent, rel_acked, rel_failed, rel_retried, rel_duplicates;
        reliable_channel->get_stats(rel_sent, rel_acked, rel_failed, rel_retried, rel_duplicates);
        
        uint64_t unrel_sent, unrel_received;
        unreliable_channel->get_stats(unrel_sent, unrel_received);
        
        std::cout << "\n========== CHANNEL STATISTICS ==========\n";
        std::cout << "[RELIABLE CHANNEL]\n"
                 << "  Sent: " << rel_sent << "\n"
                 << "  Acked: " << rel_acked << "\n"
                 << "  Failed: " << rel_failed << "\n"
                 << "  Retried: " << rel_retried << "\n"
                 << "  Duplicates: " << rel_duplicates << "\n"
                 << "  Success Rate: " << (rel_sent > 0 ? (rel_acked * 100.0 / rel_sent) : 0) << "%\n";
        
        std::cout << "[UNRELIABLE CHANNEL]\n"
                 << "  Sent: " << unrel_sent << "\n"
                 << "  Received: " << unrel_received << "\n";
        std::cout << "========================================\n\n";
    }
};

// ============ INTEGRATION VỚI SERVER ============
/*
Trong HighPerformanceGameServer class:

private:
    std::unique_ptr<DualChannelManager> channel_manager;

// Trong initialize():
bool initialize(int port) {
    // ... create socket ...
    channel_manager = std::make_unique<DualChannelManager>(server_socket);
    return true;
}

// Trong start():
void start() {
    channel_manager->start();
    // ... other threads ...
}

// Trong handle_incoming_data():
void handle_incoming_data(std::array<uint8_t, BUFFER_SIZE>& buffer) {
    sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    
    while (true) {
        int bytes = recvfrom(server_socket, (char*)buffer.data(), BUFFER_SIZE, 
                            0, (sockaddr*)&client_addr, &addr_len);
        
        if (bytes == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) break;
            continue;
        }
        
        if (bytes < 1) continue;
        
        uint8_t channel_type = buffer[0];
        
        if (channel_type == 0xFE) {
            // ACK packet - let channel manager handle
            channel_manager->process_incoming(buffer.data(), bytes, client_addr);
            continue;
        }
        
        // Parse header based on channel type
        uint8_t* ptr = buffer.data() + 1;
        
        if (channel_type == static_cast<uint8_t>(ChannelType::RELIABLE)) {
            if (bytes < 1 + sizeof(uint32_t) * 4) continue;
            
            uint32_t sequence_id = ntohl(*reinterpret_cast<uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            uint32_t player_id = ntohl(*reinterpret_cast<uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            // Send ACK & check duplicate
            channel_manager->process_incoming(buffer.data(), bytes, client_addr);
            
            // Continue parsing if not duplicate
            uint32_t match_id = ntohl(*reinterpret_cast<uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            uint32_t msg_type = ntohl(*reinterpret_cast<uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            // Create GameMessage and process...
            
        } else if (channel_type == static_cast<uint8_t>(ChannelType::UNRELIABLE)) {
            if (bytes < 1 + sizeof(uint32_t) * 3) continue;
            
            channel_manager->process_incoming(buffer.data(), bytes, client_addr);
            
            uint32_t player_id = ntohl(*reinterpret_cast<uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            uint32_t match_id = ntohl(*reinterpret_cast<uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            uint32_t msg_type = ntohl(*reinterpret_cast<uint32_t*>(ptr));
            ptr += sizeof(uint32_t);
            
            // Create GameMessage and process...
        }
    }
}

// Broadcast game state via UNRELIABLE channel
void broadcast_game_state(int match_id) {
    std::array<uint8_t, BUFFER_SIZE> buffer;
    uint8_t* ptr = buffer.data();
    
    size_t total_size;
    match->SerializeGameState(ptr, total_size);
    
    for (auto& player : match->GetAllPlayer()) {
        auto p = players.find(player->GetId());
        if (p != players.end()) {
            channel_manager->send_unreliable(
                player->GetId(), match_id, MSG_TYPE_GAME_STATE,
                buffer.data(), total_size, p->second->addr
            );
        }
    }
}

// Send critical event via RELIABLE channel
void notify_match_created(uint32_t player_id, int match_id) {
    std::array<uint8_t, 256> payload;
    // ... serialize match data ...
    
    auto p = players.find(player_id);
    if (p != players.end()) {
        channel_manager->send_reliable(
            player_id, match_id, MSG_TYPE_MATCH_CREATED,
            payload.data(), payload_size, p->second->addr
        );
    }
}

// Trong monitor_performance():
void monitor_performance() {
    // ... existing stats ...
    channel_manager->print_stats();
}

// USAGE EXAMPLES:

// 1. Critical events → RELIABLE
channel_manager->send_reliable(player_id, match_id, MSG_PLAYER_JOINED, ...);
channel_manager->send_reliable(player_id, match_id, MSG_MATCH_CREATED, ...);
channel_manager->send_reliable(player_id, match_id, MSG_REWARD_GRANTED, ...);

// 2. High-frequency updates → UNRELIABLE  
channel_manager->send_unreliable(player_id, match_id, MSG_POSITION_UPDATE, ...);
channel_manager->send_unreliable(player_id, match_id, MSG_GAME_STATE, ...);
channel_manager->send_unreliable(player_id, match_id, MSG_PROJECTILE_FIRED, ...);
*/