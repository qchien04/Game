import pygame

class Object:
    def __init__(self,x,y,width,height,image,screen):
        self.x=x
        self.y=y
        self.width=width
        self.height=height
        self.image=image
        self.screen=screen
        self.image_rect=pygame.Rect(0,0,0,0)
        self.collider=[width,height]

        self.velocity=pygame.math.Vector2(0,0)

    def draw(self):
        
        self.screen.blit(pygame.transform.scale(self.image,(self.width,self.height)),(self.x,self.y))

    def update(self):
        self.x +=self.velocity[0]
        self.y+=self.velocity[1]
        self.image_rect=self.image.get_rect(center=(self.x,self.y))
        self.draw()

    def get_center(self):

        return self.x+self.width/2, self.y+self.height/2