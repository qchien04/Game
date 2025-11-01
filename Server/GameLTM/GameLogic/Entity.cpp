#include "Entity.h"
#include <cmath>

namespace AnCom{
    Entity::Entity(float x, float y, float height, float width, float speed) : m_position{x, y}, m_height{height}, m_width{width}, m_speed{speed} {}
    Entity::~Entity() = default;
    
    const Position& Entity::GetPosition() const { return m_position; }
    
    void Entity::SetPosition(float x, float y) { m_position.x = x; m_position.y = y; }
    
    void Entity::Move(float dx, float dy) { m_position.x += dx; m_position.y += dy; }

    void Entity::Move(bool isLeft, bool isRight, bool isUp, bool isDown){
        if(isLeft) m_position.x -= m_speed;
        if(isRight) m_position.x += m_speed;
        if(isUp) m_position.y -= m_speed;
        if(isDown) m_position.y += m_speed;
    }
    
    float Entity::GetDistanceTo(float x, float y) { return std::sqrt((x - m_position.x) * (x - m_position.x) + (y - m_position.y) * (y - m_position.y)); }
    
    float Entity::GetDistanceTo(Entity& entity) { return std::sqrt((entity.GetPosition().x - m_position.x) * (entity.GetPosition().x - m_position.x) + (entity.GetPosition().y - m_position.y) * (entity.GetPosition().y - m_position.y)); }
}