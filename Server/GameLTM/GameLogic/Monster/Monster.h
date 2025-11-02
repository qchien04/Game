#pragma once
#include "Entity.h"
#include <chrono>
#include <random>

namespace AnCom {
    enum class E_MonsterState {
        IDLE = 0,
        RUN = 1,
        ATTACK = 2,
        DEAD = 3
    };

    enum class Direction {
        NONE = 0,
        UP = 1,
        DOWN = 2,
        LEFT = 3,
        RIGHT = 4
    };

    class Monster : public Entity {
    public:
        Monster(float x, float y, float height, float width, 
                float speed, int health, int id, int monster_type);
        ~Monster();

        void Update(float delta_time);
        void TakeDamage(int damage);
        bool IsDead() const { return m_current_health <= 0; }

        int GetId() const { return m_id; }
        int GetHealth() const { return m_current_health; }
        int GetMaxHealth() const { return m_max_health; }
        E_MonsterState GetState() const { return m_state; }
        Direction GetDirection() const { return m_direction; }
        int GetMonsterType() const { return m_monster_type; }

    private:
        void UpdateAI(float delta_time);
        void ChangeState(E_MonsterState new_state);
        void PickRandomDirection();

        int m_id;
        int m_monster_type; // 1 = Slime, 2 = Goblin, etc.
        
        int m_max_health;
        int m_current_health;

        E_MonsterState m_state;
        Direction m_direction;

        // AI Timer
        float m_state_timer;
        float m_idle_duration;
        float m_run_duration;

        // Random
        std::mt19937 m_rng;
    };
}
