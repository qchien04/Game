#pragma once
#include "Monster.h"
#include <vector>

namespace AnCom {
    class MonsterManager {
    public:
        MonsterManager();
        ~MonsterManager();

        void AddMonster(Monster* monster);
        void RemoveMonster(Monster* monster);
        void Update(float delta_time);
        void RemoveDeadMonsters();

        const std::vector<Monster*>& GetAllMonsters() const;

        Monster* FindMonsterAt(float x, float y, float range);

    private:
        std::vector<Monster*> monsters;
    };
}