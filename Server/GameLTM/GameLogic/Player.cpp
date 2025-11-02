#include "Player.h"

namespace AnCom{
    Player::Player(float x, float y, float height, float width, 
    float speed, int direction, float damage, 
    int health, int energy, int team, int id) : Entity(x, y, height, width, speed), m_direction(direction), m_damage(damage), 
        m_health(health), m_energy(energy), m_team(team), m_id(id) {
            this->m_current_health = health;
            this->m_current_energy = energy;
            this->m_current_speed = speed;
            this->m_current_damage = damage;
    }
    Player::~Player() = default;

    void Player::Attack(){
        
    }

    void Player::Move(bool isLeft, bool isRight, bool isUp, bool isDown) {
        if(isLeft) this->m_position.x -= this->m_current_speed;
        if(isRight) this->m_position.x += this->m_current_speed;
        if(isUp) this->m_position.y -= this->m_current_speed;
        if(isDown) this->m_position.y += this->m_current_speed;
    }
    
    

    bool Player::IsDie(){
        return this->m_current_health <= 0;
    }
}
