
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <mswsock.h>
#include <string>
#include <cstdint>   // cho uint32_t
#include <chrono>    // cho std::chrono
#include <vector>    // cho std::vector
#include <array>
#include <unordered_map>
#include <memory>


struct Player {
    uint32_t id;
    sockaddr_in addr;
    std::chrono::steady_clock::time_point last_seen;
    uint32_t sequence_num;
    bool active;
    
    Player() : id(0), sequence_num(0), active(false) {}
};

enum class MessageType : uint16_t {
    // Authentication
    LOGIN_REQUEST = 1001,
    LOGIN_RESPONSE = 1002,
    REGISTER_REQUEST = 1003,
    REGISTER_RESPONSE = 1004,
    LOGOUT_REQUEST = 1005,
    LOGOUT_RESPONSE = 1006,
    
    // Room Management
    CREATE_ROOM_REQUEST = 2001,
    CREATE_ROOM_RESPONSE = 2002,
    JOIN_ROOM_REQUEST = 2003,
    JOIN_ROOM_RESPONSE = 2004,
    LEAVE_ROOM_REQUEST = 2005,
    LEAVE_ROOM_RESPONSE = 2006,
    LIST_ROOMS_REQUEST = 2007,
    LIST_ROOMS_RESPONSE = 2008,
    ROOM_STATE_UPDATE = 2009,
    
    // Game Management
    START_GAME_REQUEST = 3001,
    START_GAME_RESPONSE = 3002,
    END_GAME_REQUEST = 3003,
    END_GAME_RESPONSE = 3004,
    GAME_READY_REQUEST = 3005,
    GAME_READY_RESPONSE = 3006,
    
    // General
    HEARTBEAT = 9001,
    ERROR_RESPONSE = 9999
};

enum class ConnectionState : uint8_t {
    CONNECTING,
    CONNECTED,
    AUTHENTICATED,
    IN_ROOM,
    IN_GAME,
    DISCONNECTING,
    DISCONNECTED
};

enum class RoomState : uint8_t {
    WAITING,
    STARTING,
    IN_PROGRESS,
    FINISHED
};

// Protocol message structure
struct ProtocolMessage {
    uint32_t length;        // Message length (big-endian)
    uint16_t type;          // Message type (big-endian)
    uint16_t sequence;      // Sequence number (big-endian)
    std::vector<uint8_t> payload;
    
    void reset() {
        length = 0;
        type = 0;
        sequence = 0;
        payload.clear();
    }
    
    void serialize(std::vector<uint8_t>& buffer) const {
        buffer.clear();
        buffer.reserve(8 + payload.size());
        
        // Length (total message size including header)
        uint32_t net_length = htonl(8 + payload.size());
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&net_length), 
                     reinterpret_cast<const uint8_t*>(&net_length) + sizeof(net_length));
        
        // Type
        uint16_t net_type = htons(type);
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&net_type), 
                     reinterpret_cast<const uint8_t*>(&net_type) + sizeof(net_type));
        
        // Sequence
        uint16_t net_sequence = htons(sequence);
        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&net_sequence), 
                     reinterpret_cast<const uint8_t*>(&net_sequence) + sizeof(net_sequence));
        
        // Payload
        if (!payload.empty()) {
            buffer.insert(buffer.end(), payload.begin(), payload.end());
        }
    }
    
    bool deserialize(const uint8_t* data, size_t size) {
        if (size < 8) return false;
        
        const uint8_t* ptr = data;
        
        // Length
        length = ntohl(*reinterpret_cast<const uint32_t*>(ptr));
        ptr += sizeof(uint32_t);
        
        // Type
        type = ntohs(*reinterpret_cast<const uint16_t*>(ptr));
        ptr += sizeof(uint16_t);
        
        // Sequence
        sequence = ntohs(*reinterpret_cast<const uint16_t*>(ptr));
        ptr += sizeof(uint16_t);
        
        // Payload
        if (length > 8) {
            size_t payload_size = length - 8;
            if (size < length) return false;
            payload.assign(ptr, ptr + payload_size);
        }
        
        return true;
    }
};

// User information
struct User {
    uint32_t user_id;
    std::string username;
    std::string password_hash;
    uint32_t room_id;
    std::chrono::steady_clock::time_point last_heartbeat;
    ConnectionState state;
    
    User() : user_id(0), room_id(0), state(ConnectionState::CONNECTED) {}
};

// Room information
struct GameRoom {
    uint32_t room_id;
    std::string room_name;
    uint32_t max_players;
    uint32_t current_players;
    uint32_t owner_id;
    RoomState state;
    std::vector<uint32_t> player_list;
    std::unordered_map<std::string, std::string> settings;
    std::chrono::steady_clock::time_point created_time;
    
    GameRoom() : room_id(0), max_players(4), current_players(0), 
                 owner_id(0), state(RoomState::WAITING) {}
};

// Cấu trúc TCPConnection dùng cho Windows
struct TCPConnection {
    SOCKET socket_fd;
    sockaddr_in client_addr;
    ConnectionState state;
    std::unique_ptr<User> user;
    
    // Buffer management
    std::vector<uint8_t> read_buffer;
    std::vector<uint8_t> write_buffer;
    size_t bytes_read;
    size_t bytes_to_write;
    size_t bytes_written;
    
    // Message parsing
    bool header_complete;
    uint32_t expected_message_length;
    
    std::chrono::steady_clock::time_point last_activity;
    uint16_t next_sequence;
    
    TCPConnection(SOCKET fd, const sockaddr_in& addr)
        : socket_fd(fd), client_addr(addr), state(ConnectionState::CONNECTING),
          bytes_read(0), bytes_to_write(0), bytes_written(0),
          header_complete(false), expected_message_length(0),
          last_activity(std::chrono::steady_clock::now()), next_sequence(1) {
        read_buffer.reserve(8192);
        write_buffer.reserve(8192);
    }
    
    ~TCPConnection() {
        if (socket_fd != INVALID_SOCKET) {
            closesocket(socket_fd);
            socket_fd = INVALID_SOCKET;
        }
    }
};

struct GameMessage {
    int32_t player_id;
    int32_t match_id;
    int32_t msg_type;
    int32_t timestamp;
    std::array<uint8_t, 1024> payload;
    size_t payload_size;
    sockaddr_in client_addr;
    
    // Add reset method to properly initialize reused messages
    void reset() {
        player_id = 0;
        msg_type = 0;
        timestamp = 0;
        payload_size = 0;
        memset(&client_addr, 0, sizeof(client_addr));
        // Don't need to clear payload array for performance
    }
};
