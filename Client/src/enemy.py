"""
Enemy class for zombies
"""
import pygame
import random
import math
from .config import Config

class Enemy(pygame.sprite.Sprite):
    def __init__(self, x, y, width, height, image_path, speed, health):
        pygame.sprite.Sprite.__init__(self)
        
        # Basic properties
        self.x = float(x)
        self.y = float(y)
        self.width = width
        self.height = height
        self.speed = speed
        self.health = health
        self.max_health = health
        self.alive = True
        
        # Load and scale image
        self.image = self._load_image(image_path, width, height)
        self.rect = self.image.get_rect(center=(x, y))
        
        # AI properties
        self.target = None
        self.attack_range = 50
        self.attack_damage = 10
        self.attack_cooldown = 1000  # milliseconds
        self.last_attack = 0
        
        # Movement properties
        self.velocity = pygame.math.Vector2(0, 0)
        self.acceleration = 0.5
        self.friction = 0.9
        
        # Animation properties (if needed)
        self.animation_speed = 200
        self.last_animation_update = pygame.time.get_ticks()
        
        # Pathfinding
        self.stuck_timer = 0
        self.stuck_threshold = 2000  # 2 seconds
        self.last_position = pygame.math.Vector2(x, y)
        
    def _load_image(self, image_path, width, height):
        """Load and scale enemy image"""
        try:
            image = pygame.image.load(image_path).convert_alpha()
            return pygame.transform.scale(image, (width, height))
        except pygame.error:
            # Create placeholder if image not found
            placeholder = pygame.Surface((width, height))
            placeholder.fill((255, 0, 0))  # Red placeholder
            return placeholder
            
    def update(self, player):
        """Update enemy state"""
        if not self.alive:
            return
            
        self.target = player
        
        # Update AI behavior
        self._update_ai()
        
        # Update movement
        self._update_movement()
        
        # Update position
        self._update_position()
        
        # Check if stuck and handle it
        self._check_stuck()
        
    def _update_ai(self):
        """Update AI decision making"""
        if not self.target or not self.target.alive:
            return
            
        # Calculate distance to target
        distance = self._distance_to_target()
        
        if distance <= self.attack_range:
            self._attack_behavior()
        else:
            self._chase_behavior()
            
    def _chase_behavior(self):
        """AI behavior for chasing player"""
        if not self.target:
            return
            
        # Calculate direction to target
        target_pos = pygame.math.Vector2(self.target.rect.centerx, self.target.rect.centery)
        current_pos = pygame.math.Vector2(self.x, self.y)
        
        direction = target_pos - current_pos
        
        if direction.length() > 0:
            direction.normalize_ip()
            
            # Apply acceleration towards target
            self.velocity += direction * self.acceleration
            
            # Limit max speed
            if self.velocity.length() > self.speed:
                self.velocity.scale_to_length(self.speed)
                
    def _attack_behavior(self):
        """AI behavior for attacking player"""
        current_time = pygame.time.get_ticks()
        
        if current_time - self.last_attack >= self.attack_cooldown:
            self._perform_attack()
            self.last_attack = current_time
            
        # Slow down when attacking
        self.velocity *= 0.5
        
    def _perform_attack(self):
        """Perform attack on player"""
        if self.target and self._distance_to_target() <= self.attack_range:
            # Deal damage to player
            if hasattr(self.target, 'take_damage'):
                self.target.take_damage(self.attack_damage)
                
    def _update_movement(self):
        """Update movement with physics"""
        # Apply friction
        self.velocity *= self.friction
        
        # Stop if velocity is very low
        if self.velocity.length() < 0.1:
            self.velocity = pygame.math.Vector2(0, 0)
            
    def _update_position(self):
        """Update enemy position"""
        # Move enemy
        self.x += self.velocity.x
        self.y += self.velocity.y
        
        # Update rect
        self.rect.centerx = int(self.x)
        self.rect.centery = int(self.y)
        
        # Keep enemy on screen
        self.rect.clamp_ip(pygame.Rect(0, 0, Config.SCREEN_WIDTH, Config.SCREEN_HEIGHT))
        self.x = float(self.rect.centerx)
        self.y = float(self.rect.centery)
        
    def _distance_to_target(self):
        """Calculate distance to target"""
        if not self.target:
            return float('inf')
            
        dx = self.x - self.target.rect.centerx
        dy = self.y - self.target.rect.centery
        return math.sqrt(dx * dx + dy * dy)
        
    def _check_stuck(self):
        """Check if enemy is stuck and handle it"""
        current_pos = pygame.math.Vector2(self.x, self.y)
        
        # Check if position changed significantly
        if current_pos.distance_to(self.last_position) < 1.0:
            self.stuck_timer += pygame.time.get_ticks() - self.last_animation_update
        else:
            self.stuck_timer = 0
            self.last_position = current_pos
            
        # If stuck too long, add random movement
        if self.stuck_timer > self.stuck_threshold:
            random_direction = pygame.math.Vector2(
                random.uniform(-1, 1),
                random.uniform(-1, 1)
            )
            if random_direction.length() > 0:
                random_direction.normalize_ip()
                self.velocity += random_direction * self.speed * 0.5
                
            self.stuck_timer = 0
            
        self.last_animation_update = pygame.time.get_ticks()
        
    def take_damage(self, damage):
        """Take damage and return True if enemy died"""
        self.health -= damage
        
        if self.health <= 0:
            self.alive = False
            return True  # Enemy died
            
        return False  # Enemy still alive
        
    def heal(self, amount):
        """Heal enemy"""
        self.health = min(self.health + amount, self.max_health)
        
    def draw(self, screen):
        """Draw enemy to screen"""
        if self.alive:
            screen.blit(self.image, self.rect)
            
            # Draw health bar if damaged
            if self.health < self.max_health:
                self._draw_health_bar(screen)
                
    def _draw_health_bar(self, screen):
        """Draw health bar above enemy"""
        bar_width = self.width
        bar_height = 5
        bar_x = self.rect.x
        bar_y = self.rect.y - 10
        
        # Background (red)
        bg_rect = pygame.Rect(bar_x, bar_y, bar_width, bar_height)
        pygame.draw.rect(screen, (255, 0, 0), bg_rect)
        
        # Foreground (green)
        health_ratio = self.health / self.max_health
        fg_width = int(bar_width * health_ratio)
        if fg_width > 0:
            fg_rect = pygame.Rect(bar_x, bar_y, fg_width, bar_height)
            pygame.draw.rect(screen, (0, 255, 0), fg_rect)
            
    def get_rect(self):
        """Get enemy collision rect"""
        return self.rect
        
    def collides_with(self, other_rect):
        """Check collision with another rect"""
        return self.rect.colliderect(other_rect)
        
    def set_target(self, target):
        """Set enemy target"""
        self.target = target
        
    def is_alive(self):
        """Check if enemy is alive"""
        return self.alive
        
    def get_health_ratio(self):
        """Get health as ratio (0.0 to 1.0)"""
        return self.health / self.max_health if self.max_health > 0 else 0.0


class ZombieEnemy(Enemy):
    """Specialized zombie enemy with unique behaviors"""
    
    def __init__(self, x, y, zombie_type=1):
        # Zombie type properties
        zombie_stats = {
            1: {'size': (60, 60), 'speed': 2, 'health': 3},
            2: {'size': (65, 65), 'speed': 2.5, 'health': 4},
            3: {'size': (70, 70), 'speed': 1.5, 'health': 5},
            4: {'size': (75, 75), 'speed': 3, 'health': 3},
            5: {'size': (100, 100), 'speed': 1, 'health': 25},  # Boss
            6: {'size': (120, 120), 'speed': 1, 'health': 30}   # Boss
        }
        
        stats = zombie_stats.get(zombie_type, zombie_stats[1])
        image_path = f'{Config.ZOMBIES_PATH}{zombie_type}_Zombie.png'
        
        super().__init__(
            x, y, 
            stats['size'][0], stats['size'][1],
            image_path,
            stats['speed'], 
            stats['health']
        )
        
        self.zombie_type = zombie_type
        self.is_boss = zombie_type in [5, 6]
        
        # Boss specific properties
        if self.is_boss:
            self.attack_range = 80
            self.attack_damage = 20
            self.attack_cooldown = 800