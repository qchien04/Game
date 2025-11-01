"""
Audio management system
"""
import pygame
from .config import Config

class AudioManager:
    def __init__(self):
        pygame.mixer.init()
        self.sounds = {}
        self.is_muted = False
        self._load_sounds()
        
    def _load_sounds(self):
        """Load all game sounds"""
        sound_files = {
            'bg_music_home': 'cottagecore-17463.mp3',
            'hit': 'sound bắn trúg zombie.mp3',
            'bg_music': 'nền.mp3',
            'shot': 'tiếng súng .mp3',
            'start': 'fight khi start game.mp3',
            'died': 'Mình chết.mp3'
        }
        
        for name, filename in sound_files.items():
            try:
                sound = pygame.mixer.Sound(f'{Config.SOUND_PATH}{filename}')
                sound.set_volume(Config.VOLUMES.get(name, 0.5))
                self.sounds[name] = sound
            except pygame.error as e:
                print(f"Could not load sound {filename}: {e}")
                
    def play_sound(self, sound_name, loops=0, fade_ms=0):
        """Play a specific sound"""
        if not self.is_muted and sound_name in self.sounds:
            if loops == -1:  # Loop indefinitely
                self.sounds[sound_name].play(loops=loops, fade_ms=fade_ms)
            else:
                self.sounds[sound_name].play()
                
    def stop_sound(self, sound_name):
        """Stop a specific sound"""
        if sound_name in self.sounds:
            self.sounds[sound_name].stop()
            
    def stop_all_sounds(self):
        """Stop all currently playing sounds"""
        pygame.mixer.stop()
        
    def set_volume(self, sound_name, volume):
        """Set volume for a specific sound"""
        if sound_name in self.sounds:
            self.sounds[sound_name].set_volume(volume)
            
    def mute_all(self):
        """Mute all sounds"""
        self.is_muted = True
        self.stop_all_sounds()
        
    def unmute_all(self):
        """Unmute all sounds"""
        self.is_muted = False
        
    def toggle_mute(self):
        """Toggle mute state"""
        if self.is_muted:
            self.unmute_all()
        else:
            self.mute_all()
            
    # Convenience methods for specific sounds
    def play_bg_music(self):
        """Play background music"""
        self.play_sound('bg_music', loops=-1, fade_ms=3)
        
    def play_home_music(self):
        """Play home screen music"""
        self.play_sound('bg_music_home', loops=-1, fade_ms=5)
        
    def stop_home_music(self):
        """Stop home screen music"""
        self.stop_sound('bg_music_home')
        
    def play_shot(self):
        """Play shooting sound"""
        self.stop_sound('bg_music')  # Brief pause in bg music
        self.play_sound('shot')
        self.play_bg_music()  # Resume bg music
        
    def play_hit(self):
        """Play hit sound"""
        self.play_sound('hit')
        
    def play_start_music(self):
        """Play start game music"""
        self.play_sound('start')
        
    def play_death_music(self):
        """Play death music"""
        self.stop_sound('bg_music')
        self.play_sound('died')
        self.play_home_music()