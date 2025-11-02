
from pathlib import Path
class Config:
    SERVER_IP="192.168.2.45"
    SERVER_PORT_TCP=8112
    SERVER_PORT_UDP=8080


    PLAYERID=1
    ENEMYID=2
    MATCHID=0

    # Screen settings
    SCREEN_WIDTH = 1360
    SCREEN_HEIGHT = 780
    FPS = 30
    
    # Game settings
    ROWS = 16
    COLS = 29
    TILE_SIZE = SCREEN_HEIGHT // ROWS
    TILE_TYPES = 21
    SCROLL = 200
    
    # Player settings
    PLAYER_SPEED = 5
    PLAYER_HEALTH = 100
    PLAYER_SIZE = (40, 80)
    
    # Bullet settings
    BULLET_SPEED = 6
    BULLET_SIZE = (30, 34)
    
    # Colors
    BG_COLOR = '#89A477'
    WHITE = 'White'
    BLACK = 'Black'
    
    # Paths
    PREFIX=Path(__file__).resolve().parent.parent.__str__()+"/resources/"
    print(PREFIX)
    ASSETS_PATH = PREFIX+ 'asset2/'
    SOUND_PATH = PREFIX+ 'Sound/'
    FONT_PATH = PREFIX+'fonts/VCR_OSD_MONO_1.001.ttf'
    PLAYER_ANIMATIONS_PATH = PREFIX+'player/player_'
    BULLET_PATH = PREFIX+'player/player_bullet/'
    ZOMBIES_PATH = PREFIX+'Zombies/'
    MAP_PLAY_PATH = PREFIX+'map/'
    
    # Game mechanics
    ANIMATION_COOLDOWN = 100
    ENEMY_SPAWN_DELAY = 10000
    MAX_ENEMY_WAVES = 9
    
    # Audio volumes
    VOLUMES = {
        'bg_music': 0.1,
        'bg_music_home': 0.4,
        'shot': 0.2,
        'hit': 0.3,
        'died': 0.5,
        'start': 0.3
    }