import pygame
import os
from .config import Config

class Player(pygame.sprite.Sprite):
    def __init__(self, x, y):
        pygame.sprite.Sprite.__init__(self)
        
        # Basic properties
        self.alive = True
        self.speed = Config.PLAYER_SPEED
        self.health = Config.PLAYER_HEALTH
        self.max_health = self.health
        
        # Animation properties
        self.action = 0
        self.frame_index = 0
        self.animation_list = []
        self.update_time = pygame.time.get_ticks()
        self.direction = 1
        self.flip = False
        
        # Load animations
        self._load_animations()
        
        # Set initial image and rect
        self.image = self.animation_list[self.action][self.frame_index]
        self.rect = self.image.get_rect(center=(x, y))
        
        self.x = x
        self.y = y
        self.width = self.image.get_width()
        self.height = self.image.get_height()
        
    def _load_animations(self):
        """Load all player animations"""
        animation_types = ['left', 'right', 'up', 'down']
        
        for animation in animation_types:
            temp_list = []
            animation_path = f'{Config.PLAYER_ANIMATIONS_PATH}{animation}'
            
            if os.path.exists(animation_path):
                num_frames = len(os.listdir(animation_path))
                
                for i in range(num_frames):
                    img_path = f'{animation_path}/{animation}_{i + 1}.png'
                    img = pygame.image.load(img_path).convert_alpha()
                    img = pygame.transform.scale(img, Config.PLAYER_SIZE)
                    temp_list.append(img)
                    
            self.animation_list.append(temp_list)
            
    def update_animation(self):
        """Update player animation"""
        if self.animation_list and self.action < len(self.animation_list):
            self.image = self.animation_list[self.action][self.frame_index]
            
            if pygame.time.get_ticks() - self.update_time > Config.ANIMATION_COOLDOWN:
                self.frame_index += 1
                self.update_time = pygame.time.get_ticks()
                
            if self.frame_index >= len(self.animation_list[self.action]):
                self.frame_index = 0
                
    def update_action(self, new_action):
        """Update player action/animation state"""
        if new_action != self.action:
            self.action = new_action
            self.frame_index = 0
            self.update_time = pygame.time.get_ticks()
            
    def move(self, moving_left, moving_right, moving_up, moving_down):
        """Handle player movement with collision detection"""
        screen_scroll = 0
        dx = dy = 0
        
        # Calculate movement
        if moving_left:
            dx -= self.speed
            self.direction = -1
        if moving_right:
            dx += self.speed
            self.direction = 1
        if moving_up:
            dy -= self.speed
        if moving_down:
            dy += self.speed
            
        # Collision detection would go here
        # (Requires world reference)
        
        # Update position
        self.rect.x += dx
        self.rect.y += dy
        self.x += dx
        self.y += dy
        
        # Handle screen scrolling
        # if (self.rect.right > Config.SCREEN_WIDTH - Config.SCROLL or 
        #     self.rect.left < Config.SCROLL):
        #     self.rect.x -= dx
        #     screen_scroll = -dx
            
        # return screen_scroll
        
    def check_alive(self):
        """Check if player is still alive"""
        if self.health <= 0:
            self.health = 0
            self.speed = 0
            self.alive = False
            
    def take_damage(self, damage):
        """Take damage and check if still alive"""
        self.health -= damage
        self.check_alive()
        return not self.alive
        
    def heal(self, amount):
        """Heal player"""
        self.health = min(self.health + amount, self.max_health)
        
    def update(self):
        """Update player state"""
        self.update_animation()
        self.check_alive()
    
    def set_position(self,x,y):
        self.rect.x =x
        self.rect.y = y
        self.x=x
        self.y=y
        
    def draw(self, screen):
        """Draw player to screen"""
        if self.action != -1:
            screen.blit(self.image, self.rect)
        else:
            # Draw default image when not moving
            default_img = pygame.image.load(Config.PLAYER_ANIMATIONS_PATH + 'down/down_2.png').convert_alpha()
            default_img = pygame.transform.scale(default_img, Config.PLAYER_SIZE)
            screen.blit(default_img, self.rect)
            
    def reset(self):
        """Reset player to initial state"""
        self.alive = True
        self.health = Config.PLAYER_HEALTH
        self.speed = Config.PLAYER_SPEED
        self.action = 0
        self.frame_index = 0