from .config import Config
import pygame
import csv

class World():
    def __init__(self,screen,level=1):
        # Chỉ số map
        self.screen = screen
        self.level = level

        # Chỉ số map
        self.world_data = []
        self._load_world_data(level)
        
        # Load các thành phần ảnh của map
        self.img_list = []
        self._load_img()
        

        #chướng ngại vật 
        self.obstancle_list = []

        # Mặt đất thông thường
        self.tile_list = []

        # Scroll
        self.screen_scroll = 0

        self.process_data()

    def _load_world_data(self,level):
        self.world_data = [[-1] * Config.COLS for _ in range(Config.ROWS + 1)]

        with open(Config.MAP_PLAY_PATH + f'level{level}_data.csv', newline='') as csvfile:
            reader = csv.reader(csvfile, delimiter=',')
            for x, row in enumerate(reader):
                for y, tile in enumerate(row):
                    self.world_data[x][y] = int(tile)
    
    def _load_img(self):
        for x in range(Config.TILE_TYPES):
            img = pygame.image.load(Config.ASSETS_PATH + f'{x}.png')
            img = pygame.transform.scale(img, (Config.TILE_SIZE, Config.TILE_SIZE))
            self.img_list.append(img)

    def process_data(self):
        # Số cột trong 1 hàng map
        self.level_length = len(self.world_data[0])

        for y, row in enumerate(self.world_data):
            for x, tile in enumerate(row):
                img = self.img_list[tile]
                img_rect = img.get_rect()
                img_rect.x = x * Config.TILE_SIZE
                img_rect.y = y * Config.TILE_SIZE
                tile_data = (img, img_rect)
                if tile == 16:
                    self.obstancle_list.append(tile_data)  
                else:
                    self.tile_list.append(tile_data)
    def draw(self):
        for tile in self.obstancle_list:
            tile[1][0] += self.screen_scroll
            self.screen.blit(tile[0], tile[1])
        for tile in self.tile_list:
            tile[1][0] += self.screen_scroll
            self.screen.blit(tile[0], tile[1])
    
    def check_collision(self, rect, dx, dy):
        """Check collision with obstacles"""
        collision_x = collision_y = False
        
        for tile in self.obstacle_list:
            if tile[1].colliderect(rect.x + dx, rect.y, rect.width, rect.height):
                collision_x = True
            if tile[1].colliderect(rect.x, rect.y + dy, rect.width, rect.height):
                collision_y = True
                
        return collision_x, collision_y
        
    def update_scroll(self, screen_scroll):
        """Update world scrolling"""
        self.screen_scroll = screen_scroll
        self.bg_scroll -= screen_scroll
        
        # Update tile positions
        for tile in self.obstacle_list + self.tile_list:
            tile[1].x += screen_scroll
   