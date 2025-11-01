#include "Bullet.h"
#include <cmath>
#include <iostream>
namespace AnCom{
    Bullet::Bullet(float x, float y,float height, float width, 
                    float target_x, float target_y, 
                    float speed, float maxFlightDistance):Entity(x, y, height, width, speed){
        this->src_x = x;
        this->src_y = y;
        this->target_x = target_x;
        this->target_y = target_y;
        this->maxFlightDistance=maxFlightDistance;

        float dx = target_x - x;
        float dy = target_y - y;
        Vector2 velocity;
        float length = std::sqrt(dx * dx + dy * dy);
        if (length != 0) {
            velocity.x = dx / length * speed;
            velocity.y = dy / length * speed;
        } else {
            velocity.x = 1;
            velocity.y = 1;
        }
        this->velocity = velocity;

        std::cout<<"x:"<<x<<std::endl;
        std::cout<<"y:"<<y<<std::endl;
        std::cout<<"target x:"<<target_x<<std::endl;
        std::cout<<"target_y:"<<target_y<<std::endl;
        std::cout<<"speed:"<<speed<<std::endl;
        std::cout<<"vector x,y:"<<velocity.x<<","<<velocity.y<<std::endl;

    }

    Bullet::~Bullet() = default;

    bool Bullet::Update(){
        Move(velocity.x, velocity.y);
        float distance=GetDistanceTo(src_x, src_y);
        if(distance>maxFlightDistance){
            return true;
        }
        return false;
    }

    void Bullet::Debug(){
        std::cout<<"x:"<<this->m_position.x<<"------y:"<<this->m_position.y<<std::endl;
    }
}
