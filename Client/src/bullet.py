import pygame
from .config import Config

class Bullet:
    def __init__(self, x, y, target_x=0, target_y=0, image_path=Config.BULLET_PATH + "bullet_A.png", speed=10):
        self.speed = speed

        self.image = pygame.image.load(image_path).convert_alpha()
        self.image = pygame.transform.scale(self.image, (20, 20))
        self.image_rect = self.image.get_rect(center=(x, y))

        # Dùng Vector2 lưu vị trí thực
        self.pos = pygame.math.Vector2(x, y)
        target_pos = pygame.math.Vector2(target_x, target_y)
        direction = target_pos - self.pos
        if direction.length() != 0:
            direction = direction.normalize()
        self.velocity = direction * speed

    def update(self):
        # Di chuyển bằng Vector2
        self.pos += self.velocity
        self.image_rect.center = (round(self.pos.x), round(self.pos.y))

        # Xóa bullet nếu ra ngoài màn hình
        if (self.image_rect.right < 0 or self.image_rect.left > Config.SCREEN_WIDTH or
                self.image_rect.bottom < 0 or self.image_rect.top > Config.SCREEN_HEIGHT):
            return False 
        return True

    def draw(self, screen):
        screen.blit(self.image, self.image_rect)


"""
Bullet class for projectiles
"""
import pygame
from .config import Config

class Bullet2(pygame.sprite.Sprite):
    def __init__(self, x, y, target_x, target_y, bullet_type='D'):
        pygame.sprite.Sprite.__init__(self)
        
        # Basic properties
        self.bullet_type = bullet_type
        self.speed = Config.BULLET_SPEED
        self.damage = self._get_damage()
        
        # Load bullet image based on type
        self.image = self._load_bullet_image()
        self.rect = self.image.get_rect(center=(x, y))
        
        # Position
        self.x = float(x)
        self.y = float(y)
        
        # Calculate velocity
        self.velocity = self._calculate_velocity(x, y, target_x, target_y)
        
        # Lifetime (to prevent bullets from existing forever)
        self.lifetime = 3000  # 3 seconds in milliseconds
        self.spawn_time = pygame.time.get_ticks()
        
    def _load_bullet_image(self):
        """Load bullet image based on type"""
        bullet_files = {
            'D': 'bullet_D.png',
            'C': 'bullet_C.png',
            'B': 'bullet_B.png',
            'red': 'bullet_red.png'
        }
        
        filename = bullet_files.get(self.bullet_type, 'bullet_D.png')
        
        try:
            image = pygame.image.load(f'{Config.BULLET_PATH}{filename}')
            return pygame.transform.scale(image, Config.BULLET_SIZE)
        except pygame.error:
            # Create placeholder if image not found
            placeholder = pygame.Surface(Config.BULLET_SIZE)
            placeholder.fill((255, 255, 0))  # Yellow
            return placeholder
            
    def _get_damage(self):
        """Get damage based on bullet type"""
        damage_table = {
            'D': 1,
            'C': 2,
            'B': 3,
            'red': 5
        }
        return damage_table.get(self.bullet_type, 1)
        
    def _calculate_velocity(self, start_x, start_y, target_x, target_y):
        """Calculate bullet velocity vector"""
        dx = target_x - start_x
        dy = target_y - start_y
        
        # Create velocity vector
        velocity = pygame.math.Vector2(dx, dy)
        
        # Normalize and apply speed
        if velocity.length() > 0:
            velocity.normalize_ip()
            velocity *= self.speed
            
        return velocity
        
    def update(self):
        """Update bullet position"""
        # Move bullet
        self.x += self.velocity.x
        self.y += self.velocity.y
        
        # Update rect position
        self.rect.centerx = int(self.x)
        self.rect.centery = int(self.y)
        
        # Check if bullet should be removed
        return self._should_remove()
        
    def _should_remove(self):
        """Check if bullet should be removed"""
        # Remove if off screen
        if (self.rect.right < 0 or self.rect.left > Config.SCREEN_WIDTH or
            self.rect.bottom < 0 or self.rect.top > Config.SCREEN_HEIGHT):
            return True
            
        # Remove if lifetime expired
        if pygame.time.get_ticks() - self.spawn_time > self.lifetime:
            return True
            
        return False
        
    def draw(self, screen):
        """Draw bullet to screen"""
        screen.blit(self.image, self.rect)
        
    def get_rect(self):
        """Get bullet collision rect"""
        return self.rect
        
    def collides_with(self, other_rect):
        """Check collision with another rect"""
        return self.rect.colliderect(other_rect)
        
    @staticmethod
    def get_bullet_type_from_kills(kill_count):
        """Determine bullet type based on kill count"""
        tier = kill_count // 10
        
        if tier == 0:
            return 'D'
        elif tier == 1:
            return 'C'
        elif tier == 2:
            return 'B'
        else:
            return 'red'