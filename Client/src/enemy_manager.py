"""
Enemy spawning and management system
"""
import pygame
import random
from .enemy import Enemy, ZombieEnemy
from .config import Config

class EnemyManager:
    def __init__(self):
        self.enemies = []
        self.spawn_timer = 0
        self.spawn_delay = Config.ENEMY_SPAWN_DELAY
        self.wave_count = 0
        self.last_spawn_time = pygame.time.get_ticks()
        
        # Enemy spawn positions
        self.spawn_positions = [
            (161, 173),
            (1292, 121),
            (1306, 611),
            (75, 611),
            (780, 693)
        ]
        
        # Initialize with starting enemies
        self._spawn_initial_enemies()
        
    def _spawn_initial_enemies(self):
        """Spawn initial enemies"""
        enemy1 = ZombieEnemy(161, 173, 1)
        enemy2 = ZombieEnemy(1292, 121, 2)
        enemy3 = ZombieEnemy(1306, 611, 1)
        
        self.enemies.extend([enemy1, enemy2, enemy3])
        
    def update(self, player, game_state):
        """Update enemy spawning and behavior"""
        current_time = pygame.time.get_ticks()
        
        # Check if it's time to spawn new wave
        if current_time - self.last_spawn_time >= self.spawn_delay:
            self._spawn_wave(game_state)
            self.last_spawn_time = current_time
            
        # Update all enemies
        for enemy in self.enemies[:]:
            enemy.update(player)
            
            # Remove dead enemies
            if not enemy.is_alive():
                self.enemies.remove(enemy)
                
    def _spawn_wave(self, game_state):
        """Spawn enemies based on current wave"""
        if self.wave_count < 5:
            self._spawn_early_wave()
        elif self.wave_count < 8:
            self._spawn_mid_wave()
        elif self.wave_count == 8:
            self._spawn_boss_wave()
            
        self.wave_count += 1
        game_state.enemy_waves = self.wave_count
        
    def _spawn_early_wave(self):
        """Spawn enemies for early waves (0-4)"""
        new_enemies = [
            ZombieEnemy(161, 173, random.randint(1, 2)),
            ZombieEnemy(1292, 121, random.randint(1, 3)),
            ZombieEnemy(1306, 611, random.randint(1, 2)),
            ZombieEnemy(75, 611, random.randint(1, 2))
        ]
        self.enemies.extend(new_enemies)
        
    def _spawn_mid_wave(self):
        """Spawn enemies for mid waves (5-7)"""
        new_enemies = [
            ZombieEnemy(161, 173, random.randint(2, 4)),
            ZombieEnemy(1292, 121, random.randint(2, 4)),
            ZombieEnemy(1306, 611, random.randint(1, 3)),
            ZombieEnemy(75, 611, random.randint(1, 3)),
            ZombieEnemy(780, 693, random.randint(2, 4))
        ]
        self.enemies.extend(new_enemies)
        
    def _spawn_boss_wave(self):
        """Spawn boss wave (wave 8)"""
        new_enemies = [
            ZombieEnemy(161, 173, 5),  # Boss
            ZombieEnemy(1306, 611, 6),  # Boss
            ZombieEnemy(1292, 121, random.randint(3, 4)),
            ZombieEnemy(75, 611, random.randint(2, 3)),
            ZombieEnemy(780, 693, random.randint(2, 3))
        ]
        self.enemies.extend(new_enemies)
        
    def get_enemy_count(self):
        """Get current number of enemies"""
        return len(self.enemies)
        
    def get_enemies(self):
        """Get list of all enemies"""
        return self.enemies
        
    def clear_all_enemies(self):
        """Remove all enemies"""
        self.enemies.clear()
        
    def draw_all(self, screen):
        """Draw all enemies"""
        for enemy in self.enemies:
            enemy.draw(screen)
        
    def reset(self):
        """Reset enemy manager state"""
        self.wave_count = 0
        self.last_spawn_time = pygame.time.get_ticks()
        self.clear_all_enemies()
        self._spawn_initial_enemies()
        
    def spawn_enemy_at_position(self, x, y, width, height, image_path, speed, health):
        """Spawn a single enemy at specified position"""
        enemy = Enemy(x, y, width, height, image_path, speed, health)
        return enemy
        
    def get_enemy_count(self):
        """Get current number of enemies"""
        return len(self.enemies)
        
    def clear_all_enemies(self):
        """Remove all enemies"""
        self.clear()
        
    def reset(self):
        """Reset enemy manager state"""
        self.wave_count = 0
        self.last_spawn_time = pygame.time.get_ticks()
        self.clear_all_enemies()
        self._spawn_initial_enemies()