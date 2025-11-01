from .bullet import Bullet

class BulletManager:
    def __init__(self):
        self.bullets = []

    def shoot(self, start_pos, target_pos, kill_count=0):
        x, y = start_pos
        target_x, target_y = target_pos

        # Có thể tăng tốc độ hoặc bắn nhiều bullet tùy kill_count nếu muốn nâng cấp
        bullet = Bullet(x=x, y=y, target_x=target_x, target_y=target_y,speed=10)
        self.bullets.append(bullet)

    def update(self):
        for bullet in self.bullets[:]:
            alive = bullet.update()
            if not alive:
                self.remove_bullet(bullet)

    def add_bullet(self, bullet):
        self.bullets.append(bullet)

    def remove_bullet(self, bullet):
        if bullet in self.bullets:
            self.bullets.remove(bullet)
    def clear(self):
        self.bullets.clear()
        
    def draw(self, screen):
        for bullet in self.bullets:
            bullet.draw(screen)