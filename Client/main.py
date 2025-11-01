
import pygame
import sys
from src.game_manager import GameManager

def main():
    pygame.init()
    
    # Initialize game manager
    game_manager = GameManager()
    
    # Main game loop
    game_manager.run()
    
    # Cleanup
    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()