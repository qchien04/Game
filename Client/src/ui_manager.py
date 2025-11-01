"""
UI management for menus, HUD and buttons
"""
import pygame
import sys
from .config import Config
from .button import Button
from .game_state import GameState
from .TextInputBox import TextInputBox
from .tcp_connect import ConnectionState
from .tcp_connect import Room
from .tcp_connect import GameClient
class UIManager:
    def __init__(self, screen):
        self.screen = screen
        self.game_state= GameState()
        self._load_assets()
        self._create_buttons()
        self.username_box = TextInputBox(450, 250, 300, 40, self.font_24)
        self.password_box = TextInputBox(450, 320, 300, 40, self.font_24, is_password=True)
        self.login_button = Button(450, 400, self.button_images['start'], 1)
        self.login_error = ''
        # if len(sys.argv) != 3:
        #     print("Usage: python client.py <server_host> <tcp_port>")
        #     sys.exit(1)
        
        # host = sys.argv[1]
        # tcp_port = int(sys.argv[2])
        # udp_port = tcp_port + 1  # Assume UDP port is TCP port + 1
    
        self.client_connect= GameClient(Config.SERVER_IP, Config.SERVER_PORT_TCP)

        if not self.client_connect.connect():
            print("Failed to connect to server")
            sys.exit(1)
        
    def _load_assets(self):
        """Load UI assets"""
        # Fonts
        self.font_48 = pygame.font.Font(Config.FONT_PATH, 48)
        self.font_24 = pygame.font.Font(Config.FONT_PATH, 24)
        
        # Button images
        self.button_images = {
            'start': pygame.transform.scale(
                pygame.image.load(f'{Config.ASSETS_PATH}start.png').convert_alpha(), 
                (200, 111)
            ),
            'exit': pygame.transform.scale(
                pygame.image.load(f'{Config.ASSETS_PATH}exit.png'), 
                (200, 111)
            ),
            'resume': pygame.transform.scale(
                pygame.image.load(f'{Config.ASSETS_PATH}resume.png'), 
                (200, 111)
            ),
            'restart': pygame.transform.scale(
                pygame.image.load(f'{Config.ASSETS_PATH}restart.png'), 
                (200, 111)
            ),
            'menu': pygame.transform.scale(
                pygame.image.load(f'{Config.ASSETS_PATH}menu.png'), 
                (200, 111)
            ),
            'mute': pygame.transform.scale(
                pygame.image.load(f'{Config.ASSETS_PATH}mute.png'), 
                (46, 46)
            ),
            'unmute': pygame.transform.scale(
                pygame.image.load(f'{Config.ASSETS_PATH}unmute.png'), 
                (50, 50)
            )
        }
        
        # Background images
        self.bg_game = pygame.transform.scale(
            pygame.image.load(f'{Config.ASSETS_PATH}background13.png'), 
            (Config.SCREEN_WIDTH, Config.SCREEN_HEIGHT)
        )
        self.bg_menu = pygame.transform.scale(
            pygame.image.load(f'{Config.ASSETS_PATH}background11.png'), 
            (Config.SCREEN_WIDTH, Config.SCREEN_HEIGHT)
        )
        
        # Portal
        self.portal_surface = pygame.transform.scale(
            pygame.image.load(f'{Config.ASSETS_PATH}portal.png').convert_alpha(), 
            (120, 120)
        )
        
        # Hearts for health display
        self.red_heart = pygame.transform.scale(
            pygame.image.load(f'{Config.ASSETS_PATH}red-heart.png'), 
            (30, 30)
        )
        self.black_heart = pygame.transform.scale(
            pygame.image.load(f'{Config.ASSETS_PATH}black-heart.png'), 
            (30, 30)
        )
        
        # Text surfaces
        self.win_text = self.font_48.render('YOU WIN!', False, Config.WHITE)
        self.win_rect = self.win_text.get_rect(
            center=(Config.SCREEN_WIDTH * 0.5, Config.SCREEN_HEIGHT * 0.2)
        )
        
    def _create_buttons(self):
        """Create button objects"""
        center_x = Config.SCREEN_WIDTH // 2 - 100
        
        self.buttons = {
            'start': Button(center_x, Config.SCREEN_HEIGHT // 2 + 50, 
                                 self.button_images['start'], 1),
            'exit': Button(center_x, Config.SCREEN_HEIGHT // 2 - 150, 
                                self.button_images['exit'], 1),
            'restart': Button(center_x, Config.SCREEN_HEIGHT // 2 - 150, 
                                   self.button_images['restart'], 1),
            'menu': Button(center_x, Config.SCREEN_HEIGHT // 2 - 300, 
                                self.button_images['menu'], 1),
            'resume': Button(center_x, Config.SCREEN_HEIGHT // 2 - 300, 
                                  self.button_images['resume'], 1),
            'mute': Button(Config.SCREEN_WIDTH - 60, 10, 
                                self.button_images['mute'], 1),
            'unmute': Button(Config.SCREEN_WIDTH - 60, 10, 
                                  self.button_images['unmute'], 1)
        }
        
    def handle_start_button(self):
        """Handle start button click"""
        return self.buttons['start'].draw(self.screen)

    def handle_exit_button(self):
        """Handle exit button click"""
        return self.buttons['exit'].draw(self.screen)
        
    def handle_restart_button(self):
        """Handle restart button click"""
        return self.buttons['restart'].draw(self.screen)
        
    def handle_menu_button(self):
        """Handle menu button click"""
        return self.buttons['menu'].draw(self.screen)
        
    def handle_resume_button(self):
        """Handle resume button click"""
        return self.buttons['resume'].draw(self.screen)
        
    def handle_mute_button(self, audio_manager):
        """Handle mute/unmute button"""
        if audio_manager.is_muted:
            if self.buttons['mute'].draw(self.screen):
                audio_manager.unmute_all()
                return True
        else:
            if self.buttons['unmute'].draw(self.screen):
                audio_manager.mute_all()
                return True
        return False
        
    def render_menu(self):
        """Render main menu"""
        pygame.mouse.set_visible(True)
        self.screen.blit(self.bg_menu, (0, 0))
        if self.game_state.state == ConnectionState.CONNECTED:
            self.render_login_screen()
            return 
        elif self.game_state.state == ConnectionState.AUTHENTICATED:
            self.render_list_room()
        elif self.client_connect.state == ConnectionState.IN_ROOM:
            self.render_room()
        elif self.client_connect.state == ConnectionState.IN_GAME:
            self.game_state.start_game=True

    def render_list_room(self):
        """Render danh sách phòng chơi"""
        self.screen.blit(self.bg_menu, (0, 0))
        pygame.mouse.set_visible(True)

        title_surface = self.font_48.render("Danh sách phòng", True, (255, 255, 255))
        self.screen.blit(title_surface, (Config.SCREEN_WIDTH // 2 - title_surface.get_width() // 2, 50))

        # Render từng phòng
        start_y = 150
        spacing = 70
        self.join_buttons = []  # reset danh sách nút join mỗi frame

        for index, room in enumerate(self.rooms):
            y = start_y + index * spacing
            info_text = f"{room.room_name} | {room.current_players}/{room.max_players} người"
            info_surface = self.font_24.render(info_text, True, (255, 255, 255))
            self.screen.blit(info_surface, (150, y))

            # Tạo nút Join tương ứng nếu chưa có
            join_image = pygame.transform.scale(self.button_images['start'], (100, 50))
            join_button = Button(600, y - 10, join_image, 1)

            if join_button.draw(self.screen):
                print(f"Tham gia phòng: {room.room_name}")
                self.game_state.current_room = room
                self.game_state.current_room_id = room.room_id
                self.client_connect.join_room(room.room_id)
                self.game_state.state = ConnectionState.IN_ROOM
                self.client_connect.state = ConnectionState.IN_ROOM
                break  # Chỉ xử lý 1 lần

            self.join_buttons.append(join_button)
        create_button = Button(600, Config.SCREEN_HEIGHT - 110, self.button_images['start'], 1)
        if create_button.draw(self.screen):
            print(f"Tạo phòng")
            self.client_connect.create_room("P1")
            self.game_state.current_room_id = self.client_connect.current_room_id
            self.game_state.current_room = Room(self.game_state.current_room_id, "P1", 1, 4, 0,[self.client_connect.username],self.client_connect.user_id)
            self.client_connect.current_room=self.game_state.current_room
            self.game_state.state = ConnectionState.IN_ROOM
            self.client_connect.state = ConnectionState.IN_ROOM
    def render_pause(self):
        """Render pause menu"""
        pygame.mouse.set_visible(True)
        self.screen.blit(self.bg_menu, (0, 0))
        
    def render_death(self):
        """Render death screen"""
        pygame.mouse.set_visible(True)
        self.screen.blit(self.bg_menu, (0, 0))
        
    def render_win(self):
        """Render win screen"""
        self.screen.fill(Config.BLACK)
        self.screen.blit(self.win_text, self.win_rect)
        
    def render_hud(self, player_health, kill_count):
        """Render game HUD (health, score, etc.)"""
        # Draw health hearts
        self._draw_health_bar(player_health)
        
        # Draw score
        self._draw_score(kill_count)
        
    def _draw_health_bar(self, health):
        """Draw player health as hearts"""
        # Draw red hearts first
        for i in range(10):
            x_pos = 25 + i * (27 + 10)
            self.screen.blit(self.red_heart, (x_pos, 25))
            
        # Draw black hearts based on missing health
        hearts_to_black = 10 - (health // 10)
        for i in range(hearts_to_black):
            x_pos = 25 + (9 - i) * (27 + 10)
            self.screen.blit(self.black_heart, (x_pos, 25))
            
    def _draw_score(self, kill_count):
        """Draw kill score"""
        score_surface = self.font_48.render(f'KILLS: {kill_count}', False, Config.WHITE)
        score_rect = score_surface.get_rect(topright=(Config.SCREEN_WIDTH - 30, 10))
        self.screen.blit(score_surface, score_rect)
        
    def draw_background(self, portal_rect=None, is_win=False):
        """Draw game background"""
        self.screen.fill(Config.BG_COLOR)
        self.screen.blit(self.bg_game, (0, 0))
        
        if is_win and portal_rect:
            self.screen.blit(self.portal_surface, portal_rect)
    
    def update_login_screen(self, event):
        """Render login screen"""
        pygame.mouse.set_visible(True)
        self.screen.blit(self.bg_menu, (0, 0))

        # Handle text inputs
        self.username_box.handle_event(event)
        self.password_box.handle_event(event)       

    def render_login_screen(self):
        # Draw labels
        username_label = self.font_24.render('Username:', True, (255, 0, 0))
        password_label = self.font_24.render('Password:', True, (255, 0, 0))
        self.screen.blit(username_label, (300, 255))
        self.screen.blit(password_label, (300, 325))

        self.username_box.draw(self.screen)
        self.password_box.draw(self.screen)

        # Draw login button
        if self.login_button.draw(self.screen):
            username = self.username_box.text
            password = self.password_box.text
            if self.client_connect.login(username, password):
                self.game_state.state = ConnectionState.AUTHENTICATED
                print(f"Logged in as {username}")
                Config.PLAYERID=self.client_connect.user_id
                self.client_connect.list_rooms()
                
                self.rooms=self.client_connect.rooms
            else:
                self.login_error = "Invalid credentials"

        # Show error (if any)
        if self.login_error:
            err_surface = self.font_24.render(self.login_error, True, (255, 0, 0))
            self.screen.blit(err_surface, (400, 460))

    def render_room(self):
        """Render UI khi đã vào trong phòng"""
        self.screen.blit(self.bg_menu, (0, 0))
        pygame.mouse.set_visible(True)

        room = self.client_connect.current_room

        if room is None:
            return
        # Tiêu đề
        title_surface = self.font_48.render(f"Phòng: {room.room_name}", True, (255, 255, 255))
        self.screen.blit(title_surface, (Config.SCREEN_WIDTH // 2 - title_surface.get_width() // 2, 50))

        # Thông tin phòng
        info_text = f"Số người chơi: {room.current_players}/{room.max_players}"
        info_surface = self.font_24.render(info_text, True, (255, 255, 255))
        self.screen.blit(info_surface, (Config.SCREEN_WIDTH // 2 - info_surface.get_width() // 2, 120))
         # Danh sách người chơi
        player_list_surface = self.font_24.render("Người chơi:", True, (255, 255, 255))
        self.screen.blit(player_list_surface, (100, 200))

        for i, player_name in enumerate(room.players):
            player_surface = self.font_24.render(f"- {player_name}", True, (200, 200, 200))
            self.screen.blit(player_surface, (120, 240 + i * 30))

        # Nút bắt đầu nếu là chủ phòng (giả sử user_id == room.owner_id)
        if self.client_connect.user_id == room.owner_id:
            start_surface = self.font_24.render("Bạn là chủ phòng. Bấm Start khi sẵn sàng!", True, (0, 255, 0))
            self.screen.blit(start_surface, (Config.SCREEN_WIDTH // 2 - start_surface.get_width() // 2, 180))

            if self.handle_start_button():
                print("Game bắt đầu!")
                self.client_connect.start_game()
        else:
            waiting_surface = self.font_24.render("Chờ chủ phòng bắt đầu...", True, (255, 255, 0))
            self.screen.blit(waiting_surface, (Config.SCREEN_WIDTH // 2 - waiting_surface.get_width() // 2, 180))

        # Nút thoát phòng
        back_image = pygame.transform.scale(self.button_images['menu'], (150, 70))
        back_button = Button(Config.SCREEN_WIDTH - 205, Config.SCREEN_HEIGHT - 120, back_image, 1)
        if back_button.draw(self.screen):
            print("Thoát phòng.")
            self.game_state.state = ConnectionState.AUTHENTICATED
            self.client_connect.list_rooms()
            self.rooms = self.client_connect.rooms
