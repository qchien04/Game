"""
Main game manager class
"""
import pygame
from .Object import Object
from .config import Config
from .game_state import GameState
from .audio_manager import AudioManager
from .ui_manager import UIManager
from .world import World
from .player import Player
from .bullet_manager import BulletManager
from .server_connection import TestUDPClient
from .server_connection import Action
from .bullet import Bullet

class GameManager:
    def __init__(self):

        self.udp_client = TestUDPClient()
        
        self.udp_client.initialize()
        self.udp_client.start()

        # Initialize pygame
        self.screen = pygame.display.set_mode((Config.SCREEN_WIDTH, Config.SCREEN_HEIGHT))
        pygame.display.set_caption("Ngôi trường xác sống")
        self.clock = pygame.time.Clock()
        
        # Initialize managers
        self.audio_manager = AudioManager()
        self.ui_manager = UIManager(self.screen)
        self.bullet_manager = BulletManager()
        #self.enemy_manager = EnemyManager()
        
        # Initialize game objects
        self.world = World(self.screen,level=1)
        self.player = Player(300,68)  

        self.enemy = Player(80,68)
        
        # Initialize game state
        self.game_state = GameState()
        
        # Initialize input state
        self.input_state = {
            'moving_left': False,
            'moving_right': False,
            'moving_up': False,
            'moving_down': False,
            'attacking': False
        }

        self.action = Action.NONE
        
        # Mouse target for bullet direction
        self.target = Object(0, 0, 50, 50, pygame.image.load(Config.BULLET_PATH + "tam.png"),self.screen)
        
    def run(self):
        """Main game loop"""
        while self.game_state.running:
            self.clock.tick(Config.FPS)
            
            self.handle_events()
            self.update()
            self.render()
            
    def handle_events(self):
        """Handle all game events"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.game_state.running = False
                
            # Handle menu events
            if not self.game_state.start_game:
                self._handle_menu_events(event)
            # Handle game events
            elif self.player.alive and not self.game_state.paused:
                self._handle_game_events(event)
            # Handle pause events
            elif self.game_state.paused:
                self._handle_pause_events(event)
            # Handle death/win events
            else:
                self._handle_death_win_events(event)
                
    def _handle_menu_events(self, event):
        self.ui_manager.update_login_screen(event)
        """Handle menu screen events"""
        pass  # Handled in UI manager
        
    def _handle_game_events(self, event):
        """Handle in-game events"""
        if event.type == pygame.MOUSEBUTTONDOWN:
            self.action = Action.SHOOT
            #do something
            # self.audio_manager.play_shot()
            
        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_a:
                self.input_state['moving_left'] = True
            elif event.key == pygame.K_d:
                self.input_state['moving_right'] = True
            elif event.key == pygame.K_w:
                self.input_state['moving_up'] = True
            elif event.key == pygame.K_s:
                self.input_state['moving_down'] = True
            elif event.key == pygame.K_SPACE:
                self.game_state.paused = True
                
        if event.type == pygame.KEYUP:
            if event.key == pygame.K_a:
                self.input_state['moving_left'] = False
            elif event.key == pygame.K_d:
                self.input_state['moving_right'] = False
            elif event.key == pygame.K_w:
                self.input_state['moving_up'] = False
            elif event.key == pygame.K_s:
                self.input_state['moving_down'] = False
            elif event.key == pygame.K_ESCAPE:
                self.game_state.running = False
                
    def _handle_pause_events(self, event):
        """Handle pause screen events"""
        pass  # Handled in UI manager
        
    def _handle_death_win_events(self, event):
        """Handle death/win screen events"""
        pass  # Handled in UI manager
        
    def update(self):
        """Update game state"""
        # Update mouse target
        mouse_pos = pygame.mouse.get_pos()
        self.target.x = mouse_pos[0] - self.target.width // 2
        self.target.y = mouse_pos[1] - self.target.height // 2
        
        # Handle different game states
        if not self.game_state.start_game:
            self._update_menu()
        elif self.game_state.is_win and self.player.rect.colliderect(self.world.portal_rect):
            self._update_win()
        elif self.game_state.paused:
            self._update_pause()
        else:
            self._update_game()
            
    def _update_menu(self):
        """Update menu state"""
        self.audio_manager.play_home_music()
        
        # Handle UI buttons
        # if self.ui_manager.handle_start_button():
        #     self.audio_manager.stop_home_music()
        #     self.audio_manager.play_start_music()
        #     self.game_state.start_game = True
            
        # if self.ui_manager.handle_exit_button():
        #     self.game_state.running = False
            
        # self.ui_manager.handle_mute_button(self.audio_manager)
        
    def _update_game(self):
        """Update main game state"""
        if self.player.alive:
            pygame.mouse.set_visible(False)
            self.audio_manager.play_bg_music()
            
            # Update player movement and animation
            self._update_player()
            
            # Update enemies
            #self.enemy_manager.update(self.player, self.game_state)
            
            # Update bullets
            #self.bullet_manager.update()
            
            # Handle collisions
            self._handle_collisions()
            
        else:
            self._update_death()
            
    def _update_player(self):
        """Update player state"""
        # Update animation based on movement
        if self.input_state['moving_up']:
            self.player.update_action(2)
        elif self.input_state['moving_down']:
            self.player.update_action(3)
        elif self.input_state['moving_left']:
            self.player.update_action(0)
        elif self.input_state['moving_right']:
            self.player.update_action(1)
        else:
            self.player.action = -1
            
        # Move player
        # screen_scroll = self.player.move(
        #     self.input_state['moving_left'],
        #     self.input_state['moving_right'],
        #     self.input_state['moving_up'],
        #     self.input_state['moving_down']
        # )
        
       # self.world.bg_scroll -= screen_scroll
        

            
        
    def _handle_collisions(self):
        pass
            
    def _update_death(self):
        """Update death state"""
        self.audio_manager.play_death_music()
        
        if self.ui_manager.handle_restart_button():
            self.reset_game()
            
        if self.ui_manager.handle_menu_button():
            self.reset_game()
            self.game_state.start_game = False
            
    def _update_pause(self):
        """Update pause state"""
        self.audio_manager.play_home_music()
        
        if self.ui_manager.handle_resume_button():
            self.game_state.paused = False
            
        if self.ui_manager.handle_exit_button():
            self.game_state.running = False
            
    def _update_win(self):
        """Update win state"""
        if self.ui_manager.handle_restart_button():
            self.reset_game()
            
    def reset_game(self):
        """Reset game to initial state"""
        # Clear all game objects
        self.enemies.clear()
        self.particles.clear()
        self.health_items.clear()
        self.bullet_manager.bullets.clear()
        
        # Reset game state
        self.game_state.reset()
        
        # Reset player and world
        self.world = World()
        self.player = self.world.load_level(1)
        self.player.alive = True
        self.player.health = Config.PLAYER_HEALTH
        
        # Reset managers
        #self.enemy_manager.reset()
        
    def render(self):
        """Render everything to screen"""
        if not self.game_state.start_game:
            self.ui_manager.render_menu()
        elif self.game_state.is_win and self.player.rect.colliderect(self.world.portal_rect):
            self.ui_manager.render_win()
        elif self.game_state.paused:
            self.ui_manager.render_pause()
        elif not self.player.alive:
            self.ui_manager.render_death()
        else:
            self._render_game()
            
        pygame.display.update()
        
    def _render_game(self):
        """Render main game"""
        # Draw background and world
        self.world.draw()

        self.target.draw()
        
        

        self.udp_client.send_thread(left=self.input_state['moving_left'],
            right=self.input_state['moving_right'],
            up=self.input_state['moving_up'],
            down=self.input_state['moving_down'],
            action=self.action,
            target_x=self.target.x,
            target_y=self.target.y)
        # Nhận và đồng bộ state nếu có
        self.action = Action.NONE

        with self.udp_client.lock:
            if self.udp_client.game_state and "players" in self.udp_client.game_state:

                    # if state["action"] == Action.SHOOT and state["target_x"] >0 and state["target_y"] > 0:
                    #     self.audio_manager.play_shot()
                    #     self.bullet_manager.shoot((state["x"]+self.player.width//2, state["y"]+self.player.height//2), (state["target_x"]+self.target.width//2, state["target_y"]+self.target.height//2), self.game_state.kill_count)
                for player_id in self.udp_client.game_state["players"]:
                    state = self.udp_client.game_state["players"].get(player_id)
                    if state and state["new"]==True:
                        self.udp_client.game_state["players"][player_id]["new"]=False
                        if player_id==Config.PLAYERID:
                            self.player.set_position(state["x"], state["y"])
                        else:
                            self.enemy.set_position(state["x"], state["y"])
                        

            if self.udp_client.game_state and "bullets" in self.udp_client.game_state:
                bullets = self.udp_client.game_state["bullets"]
                self.bullet_manager.clear()
                for bullet in bullets:
                    self.bullet_manager.add_bullet(Bullet(bullet[0], bullet[1]))
                

        self.player.update()
        
        # Draw player
        self.player.draw(self.screen)
        self.enemy.draw(self.screen)

        self.bullet_manager.draw(self.screen)






















        
        # Draw UI elements
        # self.ui_manager.render_hud(self.player.health, self.game_state.kill_count)
        
        # Draw all game objects
        # for obj in Z.objects:
        #     obj.draw()
            
        # # Handle particles alpha
        # for particle in Z.particles[:]:
        #     particle.image.set_alpha(particle.image.get_alpha() - 1)
        #     if particle.image.get_alpha() == 0:
        #         Z.objects.remove(particle)
        #         Z.particles.remove(particle)