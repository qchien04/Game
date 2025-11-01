import socket
import struct
import threading
import time
import json
from enum import IntEnum
from dataclasses import dataclass
from typing import Optional, List, Dict, Callable
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class MessageType(IntEnum):
    # Authentication
    LOGIN_REQUEST = 1001
    LOGIN_RESPONSE = 1002
    REGISTER_REQUEST = 1003
    REGISTER_RESPONSE = 1004
    LOGOUT_REQUEST = 1005
    LOGOUT_RESPONSE = 1006
    
    # Room Management
    CREATE_ROOM_REQUEST = 2001
    CREATE_ROOM_RESPONSE = 2002
    JOIN_ROOM_REQUEST = 2003
    JOIN_ROOM_RESPONSE = 2004
    LEAVE_ROOM_REQUEST = 2005
    LEAVE_ROOM_RESPONSE = 2006
    LIST_ROOMS_REQUEST = 2007
    LIST_ROOMS_RESPONSE = 2008
    ROOM_STATE_UPDATE = 2009
    
    # Game Management
    START_GAME_REQUEST = 3001
    START_GAME_RESPONSE = 3002
    END_GAME_REQUEST = 3003
    END_GAME_RESPONSE = 3004
    GAME_READY_REQUEST = 3005
    GAME_READY_RESPONSE = 3006
    
    # General
    HEARTBEAT = 9001
    ERROR_RESPONSE = 9999

class ConnectionState(IntEnum):
    DISCONNECTED = 0
    CONNECTING = 1
    CONNECTED = 2
    AUTHENTICATED = 3
    IN_ROOM = 4
    IN_GAME = 5

@dataclass
class ProtocolMessage:
    length: int = 0
    type: int = 0
    sequence: int = 0
    payload: bytes = b''
    
    def serialize(self) -> bytes:
        """Serialize message to bytes"""
        payload_size = len(self.payload)
        total_length = 8 + payload_size
        
        # Pack: length(4) + type(2) + sequence(2) + payload
        header = struct.pack('!IHH', total_length, self.type, self.sequence)
        return header + self.payload
    
    @classmethod
    def deserialize(cls, data: bytes) -> Optional['ProtocolMessage']:
        """Deserialize bytes to message"""
        if len(data) < 8:
            return None
            
        length, msg_type, sequence = struct.unpack('!IHH', data[:8])
        
        if len(data) < length:
            return None
            
        payload = data[8:length] if length > 8 else b''
        
        return cls(length=length, type=msg_type, sequence=sequence, payload=payload)

@dataclass
class Room:
    room_id: int
    room_name: str
    current_players: int
    max_players: int
    state: int
    players: List[str] = None
    
    def __post_init__(self):
        if self.players is None:
            self.players = []

class GameClient:
    def __init__(self, host: str = '192.168.1.40', tcp_port: int = 8112, udp_port: int = 8113):
        self.host = host
        self.tcp_port = tcp_port
        self.udp_port = udp_port
        
        # Network components
        self.tcp_socket: Optional[socket.socket] = None
        self.udp_socket: Optional[socket.socket] = None
        
        # State
        self.state = ConnectionState.DISCONNECTED
        self.user_id = 0
        self.username = ""
        self.current_room_id = 0
        self.sequence_counter = 1
        
        # Threading
        self.running = False
        self.tcp_receive_thread: Optional[threading.Thread] = None
        self.heartbeat_thread: Optional[threading.Thread] = None
        
        # Message handling
        self.message_handlers: Dict[int, Callable] = {}
        self.pending_responses: Dict[int, threading.Event] = {}
        self.response_data: Dict[int, ProtocolMessage] = {}
        
        # Data storage
        self.rooms: List[Room] = []
        self.current_room: Optional[Room] = None
        
        self._setup_message_handlers()
    
    def _setup_message_handlers(self):
        """Setup message type handlers"""
        self.message_handlers = {
            MessageType.LOGIN_RESPONSE: self._handle_login_response,
            MessageType.REGISTER_RESPONSE: self._handle_register_response,
            MessageType.LOGOUT_RESPONSE: self._handle_logout_response,
            MessageType.CREATE_ROOM_RESPONSE: self._handle_create_room_response,
            MessageType.JOIN_ROOM_RESPONSE: self._handle_join_room_response,
            MessageType.LEAVE_ROOM_RESPONSE: self._handle_leave_room_response,
            MessageType.LIST_ROOMS_RESPONSE: self._handle_list_rooms_response,
            MessageType.START_GAME_RESPONSE: self._handle_start_game_response,
            MessageType.GAME_READY_RESPONSE: self._handle_game_ready_response,
            MessageType.ROOM_STATE_UPDATE: self._handle_room_state_update,
            MessageType.HEARTBEAT: self._handle_heartbeat,
            MessageType.ERROR_RESPONSE: self._handle_error_response,
        }
    
    def connect(self) -> bool:
        """Connect to the game server"""
        try:
            # Create TCP socket
            self.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.tcp_socket.settimeout(10.0)
            self.tcp_socket.connect((self.host, self.tcp_port))
            
            # Create UDP socket
            self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            
            self.state = ConnectionState.CONNECTED
            self.running = True
            
            # Start receive thread
            self.tcp_receive_thread = threading.Thread(target=self._tcp_receive_loop, daemon=True)
            self.tcp_receive_thread.start()
            
            # Start heartbeat thread
            self.heartbeat_thread = threading.Thread(target=self._heartbeat_loop, daemon=True)
            self.heartbeat_thread.start()
            
            logger.info(f"Connected to server at {self.host}:{self.tcp_port}")
            return True
            
            
        except Exception as e:
            logger.error(f"Failed to connect: {e}")
            self.disconnect()
            return False
    
    def disconnect(self):
        """Disconnect from server"""
        self.running = False
        self.state = ConnectionState.DISCONNECTED
        
        if self.tcp_socket:
            try:
                self.tcp_socket.close()
            except:
                pass
            self.tcp_socket = None
        
        if self.udp_socket:
            try:
                self.udp_socket.close()
            except:
                pass
            self.udp_socket = None
        
        logger.info("Disconnected from server")
    
    def _get_next_sequence(self) -> int:
        """Get next sequence number"""
        seq = self.sequence_counter
        self.sequence_counter = (self.sequence_counter + 1) % 65536
        return seq
    
    def _send_message(self, msg_type: MessageType, payload: bytes = b'') -> int:
        """Send a message and return sequence number"""
        if not self.tcp_socket or self.state == ConnectionState.DISCONNECTED:
            raise ConnectionError("Not connected to server")
        
        sequence = self._get_next_sequence()
        msg = ProtocolMessage(type=msg_type, sequence=sequence, payload=payload)
        
        try:
            data = msg.serialize()
            self.tcp_socket.sendall(data)
            return sequence
        except Exception as e:
            logger.error(f"Failed to send message: {e}")
            raise
    
    def _send_message_and_wait(self, msg_type: MessageType, payload: bytes = b'', timeout: float = 10.0) -> Optional[ProtocolMessage]:
        """Send message and wait for response"""
        sequence = self._send_message(msg_type, payload)
        
        # Create event for this sequence
        event = threading.Event()
        self.pending_responses[sequence] = event
        
        try:
            if event.wait(timeout):
                return self.response_data.pop(sequence, None)
            else:
                logger.warning(f"Timeout waiting for response to sequence {sequence}")
                return None
        finally:
            self.pending_responses.pop(sequence, None)
    
    def _tcp_receive_loop(self):
        """TCP receive loop"""
        buffer = b''
        
        while self.running:
            try:
                data = self.tcp_socket.recv(8192)
                if not data:
                    logger.warning("Server closed connection")
                    break
                
                buffer += data
                
                # Process complete messages
                while len(buffer) >= 8:
                    # Read message length
                    msg_length = struct.unpack('!I', buffer[:4])[0]
                    
                    if len(buffer) < msg_length:
                        break  # Wait for more data
                    
                    # Extract complete message
                    msg_data = buffer[:msg_length]
                    buffer = buffer[msg_length:]
                    
                    # Parse message
                    msg = ProtocolMessage.deserialize(msg_data)
                    if msg:
                        self._handle_message(msg)
                
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    logger.error(f"TCP receive error: {e}")
                break
        
        self.disconnect()
    
    def _handle_message(self, msg: ProtocolMessage):
        """Handle received message"""
        # Check if this is a response to a pending request
        if msg.sequence in self.pending_responses:
            self.response_data[msg.sequence] = msg
            self.pending_responses[msg.sequence].set()
            return
        
        # Handle message by type
        handler = self.message_handlers.get(msg.type)
        if handler:
            try:
                handler(msg)
            except Exception as e:
                logger.error(f"Error handling message type {msg.type}: {e}")
        else:
            logger.warning(f"No handler for message type {msg.type}")
    
    def _heartbeat_loop(self):
        """Send heartbeat messages"""
        while self.running:
            try:
                if self.state in [ConnectionState.CONNECTED, ConnectionState.AUTHENTICATED, 
                                ConnectionState.IN_ROOM, ConnectionState.IN_GAME]:
                    self._send_message(MessageType.HEARTBEAT)
                time.sleep(15)  # Send heartbeat every 15 seconds
            except Exception as e:
                if self.running:
                    logger.error(f"Heartbeat error: {e}")
                break
    
    # Message handlers
    def _handle_login_response(self, msg: ProtocolMessage):
        """Handle login response"""
        if len(msg.payload) < 5:
            return
        
        success = msg.payload[0] == 1
        user_id = struct.unpack('!I', msg.payload[1:5])[0]
        
        if success:
            self.user_id = user_id
            self.state = ConnectionState.AUTHENTICATED
            logger.info(f"Login successful, user ID: {user_id}")
        else:
            # Extract error message
            if len(msg.payload) > 9:
                msg_len = struct.unpack('!I', msg.payload[5:9])[0]
                error_msg = msg.payload[9:9+msg_len].decode('utf-8', errors='ignore')
                logger.error(f"Login failed: {error_msg}")
    
    def _handle_register_response(self, msg: ProtocolMessage):
        """Handle register response"""
        if len(msg.payload) < 5:
            return
        
        success = msg.payload[0] == 1
        user_id = struct.unpack('!I', msg.payload[1:5])[0]
        
        if success:
            self.user_id = user_id
            self.state = ConnectionState.AUTHENTICATED
            logger.info(f"Registration successful, user ID: {user_id}")
        else:
            if len(msg.payload) > 9:
                msg_len = struct.unpack('!I', msg.payload[5:9])[0]
                error_msg = msg.payload[9:9+msg_len].decode('utf-8', errors='ignore')
                logger.error(f"Registration failed: {error_msg}")
    
    def _handle_logout_response(self, msg: ProtocolMessage):
        """Handle logout response"""
        self.state = ConnectionState.CONNECTED
        self.user_id = 0
        self.username = ""
        self.current_room_id = 0
        logger.info("Logged out successfully")
    
    def _handle_create_room_response(self, msg: ProtocolMessage):
        """Handle create room response"""
        if len(msg.payload) < 5:
            return
        
        success = msg.payload[0] == 1
        room_id = struct.unpack('!I', msg.payload[1:5])[0]
        
        if success:
            self.current_room_id = room_id
            self.state = ConnectionState.IN_ROOM
            logger.info(f"Room created successfully, room ID: {room_id}")
    
    def _handle_join_room_response(self, msg: ProtocolMessage):
        """Handle join room response"""
        if len(msg.payload) < 5:
            return
        
        success = msg.payload[0] == 1
        room_id = struct.unpack('!I', msg.payload[1:5])[0]
        
        if success:
            self.current_room_id = room_id
            self.state = ConnectionState.IN_ROOM
            logger.info(f"Joined room successfully, room ID: {room_id}")
    
    def _handle_leave_room_response(self, msg: ProtocolMessage):
        """Handle leave room response"""
        success = msg.payload[0] == 1 if msg.payload else False
        
        if success:
            self.current_room_id = 0
            self.current_room = None
            self.state = ConnectionState.AUTHENTICATED
            logger.info("Left room successfully")
    
    def _handle_list_rooms_response(self, msg: ProtocolMessage):
        """Handle list rooms response"""
        if len(msg.payload) < 4:
            return
        
        self.rooms.clear()
        ptr = 0
        
        # Read room count
        room_count = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
        ptr += 4
        
        # Read room data
        for _ in range(room_count):
            if ptr + 4 > len(msg.payload):
                break
            
            # Room ID
            room_id = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
            ptr += 4
            
            # Room name
            if ptr + 4 > len(msg.payload):
                break
            name_len = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
            ptr += 4
            
            if ptr + name_len > len(msg.payload):
                break
            room_name = msg.payload[ptr:ptr+name_len].decode('utf-8', errors='ignore')
            ptr += name_len
            
            # Player counts and state
            if ptr + 9 > len(msg.payload):
                break
            current_players = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
            ptr += 4
            max_players = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
            ptr += 4
            state = msg.payload[ptr]
            ptr += 1
            
            room = Room(room_id, room_name, current_players, max_players, state)
            self.rooms.append(room)
        
        logger.info(f"Received {len(self.rooms)} rooms")
    
    def _handle_start_game_response(self, msg: ProtocolMessage):
        """Handle start game response"""
        success = msg.payload[0] == 1 if msg.payload else False
        if success:
            logger.info("Game started successfully")
    
    def _handle_game_ready_response(self, msg: ProtocolMessage):
        """Handle game ready response"""
        success = msg.payload[0] == 1 if msg.payload else False
        if success:
            self.state = ConnectionState.IN_GAME
            logger.info("Ready for game")
    
    def _handle_room_state_update(self, msg: ProtocolMessage):
        """Handle room state update"""
        if len(msg.payload) < 4:
            return
        
        ptr = 0
        
        # Room ID
        room_id = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
        ptr += 4
        
        # Room name
        if ptr + 4 > len(msg.payload):
            return
        name_len = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
        ptr += 4
        
        if ptr + name_len > len(msg.payload):
            return
        room_name = msg.payload[ptr:ptr+name_len].decode('utf-8', errors='ignore')
        ptr += name_len
        
        # Player counts and state
        if ptr + 13 > len(msg.payload):
            return
        current_players = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
        ptr += 4
        max_players = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
        ptr += 4
        state = msg.payload[ptr]
        ptr += 1
        
        # Player list
        player_count = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
        ptr += 4
        
        players = []
        for _ in range(player_count):
            if ptr + 8 > len(msg.payload):
                break
            
            player_id = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
            ptr += 4
            
            username_len = struct.unpack('!I', msg.payload[ptr:ptr+4])[0]
            ptr += 4
            
            if ptr + username_len > len(msg.payload):
                break
            
            username = msg.payload[ptr:ptr+username_len].decode('utf-8', errors='ignore')
            ptr += username_len
            
            players.append(username)
        
        # Update current room if it's the same
        if room_id == self.current_room_id:
            self.current_room = Room(room_id, room_name, current_players, max_players, state, players)
            logger.info(f"Room {room_name} updated: {current_players}/{max_players} players")
            logger.info(f"Players: {', '.join(players)}")
    
    def _handle_heartbeat(self, msg: ProtocolMessage):
        """Handle heartbeat response"""
        pass  # Just acknowledge
    
    def _handle_error_response(self, msg: ProtocolMessage):
        """Handle error response"""
        if len(msg.payload) >= 4:
            msg_len = struct.unpack('!I', msg.payload[:4])[0]
            if len(msg.payload) >= 4 + msg_len:
                error_msg = msg.payload[4:4+msg_len].decode('utf-8', errors='ignore')
                logger.error(f"Server error: {error_msg}")
    
    # Public API methods
    def login(self, username: str, password: str, timeout: float = 10.0) -> bool:
        """Login to server"""
        if self.state != ConnectionState.CONNECTED:
            logger.error("Not connected to server")
            return False
        
        # Build payload: username_len(4) + username + password_len(4) + password
        username_bytes = username.encode('utf-8')
        password_bytes = password.encode('utf-8')
        
        payload = struct.pack('!I', len(username_bytes)) + username_bytes
        payload += struct.pack('!I', len(password_bytes)) + password_bytes
        
        response = self._send_message_and_wait(MessageType.LOGIN_REQUEST, payload, timeout)
        if response and len(response.payload) >= 1:
            success = response.payload[0] == 1
            if success:
                self.username = username
                self.state = ConnectionState.AUTHENTICATED
            return success
        
        return False
    
    def register(self, username: str, password: str, timeout: float = 10.0) -> bool:
        """Register new user"""
        if self.state != ConnectionState.CONNECTED:
            logger.error("Not connected to server")
            return False
        
        username_bytes = username.encode('utf-8')
        password_bytes = password.encode('utf-8')
        
        payload = struct.pack('!I', len(username_bytes)) + username_bytes
        payload += struct.pack('!I', len(password_bytes)) + password_bytes
        
        response = self._send_message_and_wait(MessageType.REGISTER_REQUEST, payload, timeout)
        if response and len(response.payload) >= 1:
            success = response.payload[0] == 1
            if success:
                self.username = username
                self.state = ConnectionState.AUTHENTICATED
            return success
        
        return False
    
    def logout(self, timeout: float = 5.0) -> bool:
        """Logout from server"""
        if self.state not in [ConnectionState.AUTHENTICATED, ConnectionState.IN_ROOM]:
            return False
        
        response = self._send_message_and_wait(MessageType.LOGOUT_REQUEST, b'', timeout)
        if response and len(response.payload) >= 1:
            return response.payload[0] == 1
        
        return False
    
    def create_room(self, room_name: str, max_players: int = 4, timeout: float = 10.0) -> bool:
        """Create a new room"""
        if self.state != ConnectionState.AUTHENTICATED:
            logger.error("Must be authenticated to create room")
            return False
        
        room_name_bytes = room_name.encode('utf-8')
        payload = struct.pack('!I', len(room_name_bytes)) + room_name_bytes
        payload += struct.pack('!I', max_players)
        
        response = self._send_message_and_wait(MessageType.CREATE_ROOM_REQUEST, payload, timeout)
        if response and len(response.payload) >= 1:
            return response.payload[0] == 1
        
        return False
    
    def join_room(self, room_id: int, timeout: float = 10.0) -> bool:
        """Join an existing room"""
        if self.state != ConnectionState.AUTHENTICATED:
            logger.error("Must be authenticated to join room")
            return False
        
        payload = struct.pack('!I', room_id)
        
        response = self._send_message_and_wait(MessageType.JOIN_ROOM_REQUEST, payload, timeout)
        if response and len(response.payload) >= 1:
            return response.payload[0] == 1
        
        return False
    
    def leave_room(self, timeout: float = 5.0) -> bool:
        """Leave current room"""
        if self.state != ConnectionState.IN_ROOM:
            return False
        
        response = self._send_message_and_wait(MessageType.LEAVE_ROOM_REQUEST, b'', timeout)
        if response and len(response.payload) >= 1:
            return response.payload[0] == 1
        
        return False
    
    def list_rooms(self, timeout: float = 10.0) -> List[Room]:
        """Get list of available rooms"""
        if self.state != ConnectionState.AUTHENTICATED:
            logger.error("Must be authenticated to list rooms")
            return []
        
        response = self._send_message_and_wait(MessageType.LIST_ROOMS_REQUEST, b'', timeout)
        if not response:
            return []

        payload = response.payload

        ptr = 0

        room_count = int.from_bytes(payload[ptr:ptr+4], 'big')
        ptr += 4

        rooms = []

        for _ in range(room_count):
            room_id = int.from_bytes(payload[ptr:ptr+4], 'big')
            ptr += 4

            name_len = int.from_bytes(payload[ptr:ptr+4], 'big')
            ptr += 4

            room_name = payload[ptr:ptr+name_len].decode()
            ptr += name_len

            current_players = int.from_bytes(payload[ptr:ptr+4], 'big')
            ptr += 4

            max_players = int.from_bytes(payload[ptr:ptr+4], 'big')
            ptr += 4

            state = payload[ptr]
            ptr += 1

            room = Room(room_id, room_name, current_players, max_players, state)
            rooms.append(room)

        self.rooms = rooms  # Lưu lại nếu cần dùng về sau
        return rooms
    
    def start_game(self, timeout: float = 10.0) -> bool:
        """Start game (room owner only)"""
        if self.state != ConnectionState.IN_ROOM:
            logger.error("Must be in room to start game")
            return False
        
        response = self._send_message_and_wait(MessageType.START_GAME_REQUEST, b'', timeout)
        if response and len(response.payload) >= 1:
            return response.payload[0] == 1
        
        return False
    
    def game_ready(self, timeout: float = 5.0) -> bool:
        """Mark as ready for game"""
        if self.state != ConnectionState.IN_ROOM:
            logger.error("Must be in room to mark ready")
            return False
        
        response = self._send_message_and_wait(MessageType.GAME_READY_REQUEST, b'', timeout)
        if response and len(response.payload) >= 1:
            return response.payload[0] == 1
        
        return False
    
    def send_udp_packet(self, data: bytes) -> bool:
        """Send UDP packet for real-time game data"""
        if not self.udp_socket:
            return False
        
        try:
            self.udp_socket.sendto(data, (self.host, self.udp_port))
            return True
        except Exception as e:
            logger.error(f"UDP send error: {e}")
            return False
    
    def get_state(self) -> ConnectionState:
        """Get current connection state"""
        return self.state
    
    def get_current_room(self) -> Optional[Room]:
        """Get current room info"""
        return self.current_room
    
    def is_connected(self) -> bool:
        """Check if connected to server"""
        return self.state != ConnectionState.DISCONNECTED and self.running


# Example usage and CLI interface
def main():
    """Simple CLI interface for testing the client"""
    import sys
    tcp_port = 8112
    udp_port = 8113
    # if len(sys.argv) != 3:
    #     print("Usage: python client.py <server_host> <tcp_port>")
    #     sys.exit(1)
    
    # host = sys.argv[1]
    # tcp_port = int(sys.argv[2])
    # udp_port = tcp_port + 1  # Assume UDP port is TCP port + 1

    host= "192.168.1.40"
    
    client = GameClient(host, tcp_port, udp_port)
    
    if not client.connect():
        print("Failed to connect to server")
        sys.exit(1)
    
    print(f"Connected to {host}:{tcp_port}")
    print("Commands: login <user> <pass>, register <user> <pass>, create <name>, join <id>, list, start, ready, logout, quit")
    
    try:
        while client.is_connected():
            try:
                command = input("> ").strip().split()
                if not command:
                    continue
                
                cmd = command[0].lower()
                
                if cmd == "quit":
                    break
                elif cmd == "login" and len(command) == 3:
                    username, password = command[1], command[2]
                    if client.login(username, password):
                        print(f"Logged in as {username}")
                    else:
                        print("Login failed")
                
                elif cmd == "register" and len(command) == 3:
                    username, password = command[1], command[2]
                    if client.register(username, password):
                        print(f"Registered and logged in as {username}")
                    else:
                        print("Registration failed")
                
                elif cmd == "create" and len(command) >= 2:
                    room_name = " ".join(command[1:])
                    if client.create_room(room_name):
                        print(f"Created room '{room_name}'")
                    else:
                        print("Failed to create room")
                
                elif cmd == "join" and len(command) == 2:
                    room_id = int(command[1])
                    if client.join_room(room_id):
                        print(f"Joined room {room_id}")
                    else:
                        print("Failed to join room")
                
                elif cmd == "list":
                    rooms = client.list_rooms()
                    if rooms:
                        print("\nAvailable rooms:")
                        for room in rooms:
                            print(f"  {room.room_id}: {room.room_name} ({room.current_players}/{room.max_players})")
                    else:
                        print("No rooms available")
                
                elif cmd == "start":
                    if client.start_game():
                        print("Game started")
                    else:
                        print("Failed to start game")
                
                elif cmd == "ready":
                    if client.game_ready():
                        print("Marked as ready")
                    else:
                        print("Failed to mark ready")
                
                elif cmd == "leave":
                    if client.leave_room():
                        print("Left room")
                    else:
                        print("Failed to leave room")
                
                elif cmd == "logout":
                    if client.logout():
                        print("Logged out")
                    else:
                        print("Failed to logout")
                
                elif cmd == "status":
                    print(f"State: {client.get_state().name}")
                    print(f"User ID: {client.user_id}")
                    print(f"Username: {client.username}")
                    print(f"Room ID: {client.current_room_id}")
                    if client.current_room:
                        room = client.current_room
                        print(f"Room: {room.room_name} ({room.current_players}/{room.max_players})")
                        print(f"Players: {', '.join(room.players)}")
                
                else:
                    print("Unknown command")
                    
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"Error: {e}")
    
    finally:
        client.disconnect()
        print("Disconnected")


if __name__ == "__main__":
    main()