#include "Monster.h"
#include <iostream>

namespace AnCom {
    Monster::Monster(float x, float y, float height, float width, 
                     float speed, int health, int id, int monster_type)
        : Entity(x, y, height, width, speed),
          m_id(id),
          m_monster_type(monster_type),
          m_max_health(health),
          m_current_health(health),
          m_state(E_MonsterState::IDLE),
          m_direction(Direction::NONE),
          m_state_timer(0.0f),
          m_idle_duration(3.0f),
          m_run_duration(2.0f),
          m_rng(std::random_device{}()) {
    }

    Monster::~Monster() = default;

    void Monster::Update(float delta_time) {
        if (IsDead()) {
            m_state = E_MonsterState::DEAD;
            return;
        }

        UpdateAI(delta_time);

        // Di chuyển nếu đang RUN
        if (m_state == E_MonsterState::RUN) {
            switch (m_direction) {
                case Direction::UP:    Move(0, -m_speed * delta_time); break;
                case Direction::DOWN:  Move(0, m_speed * delta_time); break;
                case Direction::LEFT:  Move(-m_speed * delta_time, 0); break;
                case Direction::RIGHT: Move(m_speed * delta_time, 0); break;
                default: break;
            }
        }
    }

    void Monster::UpdateAI(float delta_time) {
        m_state_timer += delta_time;

        switch (m_state) {
            case E_MonsterState::IDLE:
                if (m_state_timer >= m_idle_duration) {
                    ChangeState(E_MonsterState::RUN);
                    PickRandomDirection();
                    m_state_timer = 0.0f;
                }
                break;

            case E_MonsterState::RUN:
                if (m_state_timer >= m_run_duration) {
                    ChangeState(E_MonsterState::IDLE);
                    m_direction = Direction::NONE;
                    m_state_timer = 0.0f;
                }
                break;

            default:
                break;
        }
    }

    void Monster::ChangeState(E_MonsterState new_state) {
        if (m_state != new_state) {
            std::cout << "[MONSTER " << m_id << "] State: " 
                      << static_cast<int>(m_state) << " -> " 
                      << static_cast<int>(new_state) << std::endl;
            m_state = new_state;
        }
    }

    void Monster::PickRandomDirection() {
        std::uniform_int_distribution<int> dist(1, 4);
        int random_dir = dist(m_rng);
        m_direction = static_cast<Direction>(random_dir);
        
        std::cout << "[MONSTER " << m_id << "] Moving: " 
                  << static_cast<int>(m_direction) << std::endl;
    }

    void Monster::TakeDamage(int damage) {
        m_current_health -= damage;
        if (m_current_health < 0) m_current_health = 0;
        
        std::cout << "[MONSTER " << m_id << "] Took " << damage 
                  << " damage! Health: " << m_current_health 
                  << "/" << m_max_health << std::endl;
    }
}
