from .tcp_connect import ConnectionState

class GameState:
    _instance = None 

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(GameState, cls).__new__(cls)
            cls._instance._initialized = False 
        return cls._instance

    def __init__(self):
        if self._initialized:
            return  
        self._initialized = True
        self.reset()

    def reset(self):
        self.state=ConnectionState.CONNECTED
        self.login = False
        self.running = True
        self.start_game = False
        self.is_win = False
        self.paused = False
        self.level = 1
        self.kill_count = 0
        self.enemy_waves = 0
        self.is_muted = False
        
    def increment_kills(self):
        self.kill_count += 1

    def increment_waves(self):
        self.enemy_waves += 1

    def toggle_mute(self):
        self.is_muted = not self.is_muted

    def pause_game(self):
        self.paused = True

    def resume_game(self):
        self.paused = False

    def start_new_game(self):
        self.start_game = True
        self.is_win = False
        self.paused = False

    def win_game(self):
        self.is_win = True

    def quit_game(self):
        self.running = False
