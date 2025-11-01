import socket
import time
import struct
import threading
from enum import Enum
from .config import Config
from typing import override

class Action(Enum):
    NONE = 0
    SHOOT = 1
    SKILL = 2

class Move:
    def __init__(self,left,right,up,down):
        self.left=left
        self.right=right
        self.up=up
        self.down=down
    def to_bytes(self):
        return struct.pack('!iiii', self.left, self.right, self.up, self.down)
    @override
    def __str__(self) -> str:
        return f"Move(left={self.left}, right={self.right}, up={self.up}, down={self.down})"


class Payload:
    def __init__(self,move,action=Action.NONE,action_direction=1,target_x=0,target_y=0):
        self.move=move
        self.action=action
        self.action_direction=action_direction
        self.target_x=target_x
        self.target_y=target_y

    def to_bytes(self):
        return struct.pack('!iiiiiiii', self.move.left, self.move.right, self.move.up, self.move.down, self.action.value, self.action_direction,self.target_x,self.target_y)

    @override
    def __str__(self) -> str:
        return f"Payload(move={self.move}, action={self.action}, action_direction={self.action_direction}, target_x={self.target_x}, target_y={self.target_y})"

class TestUDPClient:
    id=1
    def __init__(self, server_ip=Config.SERVER_IP, server_port=Config.SERVER_PORT_UDP):
        self.server_ip = server_ip
        self.server_port = server_port
        self.client_socket = None
        self.running = False
        self.BUFFER_SIZE = 1000
        self.payload = Payload(Move(0,0,0,0),Action.SHOOT,1,0,0)

        self.lock = threading.Lock()

        self.game_state = {}

    def initialize(self):
        try:
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.client_socket.setblocking(False)
            print("Client khởi tạo")
            return True
        except Exception as e:
            print(f"Không thể khởi tạo client: {e}")
            return False

    def start(self):
        self.running = True
        recv_thread = threading.Thread(target=self.receive_thread, daemon=True)
        recv_thread.start()
        return recv_thread

    def stop(self):
        self.running = False
        if self.client_socket:
            self.client_socket.close()
        print("Client dừng")

    def send_thread(self,left=False,right=False,up=False,down=False,action=Action.NONE,action_direction=1,target_x=-1,target_y=-1):
        if left:
            left=1
        else:
            left=0
        if right:
            right=1
        else:
            right=0
        if up:
            up=1
        else:
            up=0
        if down:
            down=1
        else:
            down=0
        msg_type = 1  # Join message
        self.payload.move.left=left
        self.payload.move.right=right
        self.payload.move.up=up
        self.payload.move.down=down

        self.payload.action=action
        self.payload.action_direction=action_direction
        self.payload.target_x=target_x
        self.payload.target_y=target_y

        try:
            if TestUDPClient.id>-1:
                player_id = Config.PLAYERID
                timestamp = int(time.time() * 1000) & 0xFFFFFFFF  # uint32 ms timestamp
                match_id = Config.MATCHID
                buffer = struct.pack('!III', player_id, msg_type, match_id) + self.payload.to_bytes()
                self.client_socket.sendto(buffer, (self.server_ip, self.server_port))
                #print(f"Gửi tin nhắn tới server, player_id: {player_id}, msg_type: {msg_type}, timestamp: {match_id}, payload: {self.payload}")
                TestUDPClient.id += 1
        except Exception as e:
            print(f"Lỗi gửi dữ liệu: {e}")


    def receive_thread(self):
        while self.running:
            try:
                data, addr = self.client_socket.recvfrom(self.BUFFER_SIZE)

                offset = 0
                if len(data) < 8:
                    print(f"[WARN] Dữ liệu quá ngắn")
                    continue
                
                match_id = struct.unpack_from("!i", data, offset)[0]
                offset += 4

                player_count = struct.unpack_from("!i", data, offset)[0]
                offset += 4

                print(f"[RECV] Match ID: {match_id}, Players: {player_count}")

                players = {}
                for _ in range(player_count):
                    id = struct.unpack_from("!i", data, offset)[0]
                    offset += 4
                    x = struct.unpack_from("!f", data, offset)[0]
                    offset += 4
                    y = struct.unpack_from("!f", data, offset)[0]
                    offset += 4
                    health = struct.unpack_from("!i", data, offset)[0]
                    offset += 4

                    players[id] = {
                        "x": x,
                        "y": y,
                        "health": health,
                        "new": True
                    }

                bullet_count = struct.unpack_from("!i", data, offset)[0]
                offset += 4

                bullets = []
                for _ in range(bullet_count):
                    x = struct.unpack_from("!f", data, offset)[0]
                    offset += 4
                    y = struct.unpack_from("!f", data, offset)[0]
                    offset += 4
                    bullets.append((x, y))

                # In ra thông tin (hoặc cập nhật self.game_state)
                print("[GAME STATE]")
                for pid, p in players.items():
                    print(f" - Player {pid}: x={p['x']:.2f}, y={p['y']:.2f}, hp={p['health']}")

                print(f" - Bullets ({len(bullets)}):")
                for b in bullets:
                    print(f"    • Bullet at x={b[0]:.2f}, y={b[1]:.2f}")

                with self.lock:
                    self.game_state["match_id"] = match_id
                    self.game_state["players"] = players
                    self.game_state["bullets"] = bullets
                
            except BlockingIOError:
                pass
            except Exception as e:
                print(f"[ERROR] {e}")
            time.sleep(0.02)


# test=TestUDPClient()
# test.initialize()
# test.start()
# while True:
#     test.send_thread()
#     time.sleep(1)




