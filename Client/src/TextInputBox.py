import pygame

class TextInputBox:
    def __init__(self, x, y, w, h, font, is_password=False):
        self.rect = pygame.Rect(x, y, w, h)
        self.color = pygame.Color('black')
        self.text = ''
        self.font = font
        self.txt_surface = font.render('', True, self.color)
        self.active = False
        self.is_password = is_password

    def handle_event(self, event):
        if event.type == pygame.MOUSEBUTTONDOWN:
            # Toggle active state if clicked inside box
            self.active = self.rect.collidepoint(event.pos)
        if event.type == pygame.KEYDOWN and self.active:
            if event.key == pygame.K_RETURN:
                pass  # Submit handled externally
            elif event.key == pygame.K_BACKSPACE:
                self.text = self.text[:-1]
            else:
                self.text += event.unicode
            # Update the surface with new text
            display_text = '*' * len(self.text) if self.is_password else self.text
            self.txt_surface = self.font.render(display_text, True, self.color)

    def draw(self, screen):
        # Draw text
        screen.blit(self.txt_surface, (self.rect.x+5, self.rect.y+5))
        # Draw box
        pygame.draw.rect(screen, self.color, self.rect, 2)
