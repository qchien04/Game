#pragma once
#include "Entity.h"

namespace AnCom {
    class Player : public Entity {
    public:
        Player(float x, float y, float height, float width, 
            float speed, int direction, float damage, 
            int health, int energy, int team, int id);
        ~Player();

        const int& GetCurrentHealth() const { return m_current_health; }
        const int& GetMaxHealth() const { return m_health; }
        const int& GetCurrentEnergy() const { return m_current_energy; }
        const int& GetMaxEnergy() const { return m_energy; }
        const float& GetCurrentSpeed() const { return m_current_speed; }
        const float& GetMaxSpeed() const { return m_speed; }
        const int& GetDirection() const { return m_direction; }
        const float& GetDamage() const { return m_current_damage; }
        const float& GetMaxDamage() const { return m_damage; }
        const int& GetId() const { return m_id; }
        const int& GetTeam() const { return m_team; }

        void SetCurrentHealth(int health) { m_current_health = health; }
        void SetMaxHealth(int maxHealth) { m_health = maxHealth; }
        void SetCurrentEnergy(int energy) { m_current_energy = energy; }
        void SetMaxEnergy(int maxEnergy) { m_energy = maxEnergy; }
        void SetCurrentSpeed(float speed) { m_current_speed = speed; }
        void SetMaxSpeed(float maxSpeed) { m_speed = maxSpeed; }
        void SetDirection(int direction) { m_direction = direction; }
        void SetDamage(float damage) { m_current_damage = damage; }
        void SetMaxDamage(float maxDamage) { m_damage = maxDamage; }

        void Attack();
        bool IsDie();

        void Move(bool isLeft, bool isRight, bool isUp, bool isDown) override;
        
    private:
        int m_health;
        int m_current_health;

        int m_energy;
        int m_current_energy;

        float m_current_speed;

        int m_direction;

        float m_damage;
        float m_current_damage;

        int m_team;
        int m_id;
    };
}