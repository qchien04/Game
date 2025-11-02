#include "MonsterManager.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace AnCom {
    MonsterManager::MonsterManager() = default;

    MonsterManager::~MonsterManager() {
        for (auto monster : monsters) {
            delete monster;
        }
        monsters.clear();
    }

    void MonsterManager::AddMonster(Monster* monster) {
        monsters.push_back(monster);
    }

    void MonsterManager::RemoveMonster(Monster* monster) {
        monsters.erase(std::remove(monsters.begin(), monsters.end(), monster), 
                      monsters.end());
    }

    void MonsterManager::Update(float delta_time) {
        for (auto monster : monsters) {
            monster->Update(delta_time);
        }
    }

    void MonsterManager::RemoveDeadMonsters() {
        for (auto it = monsters.begin(); it != monsters.end();) {
            if ((*it)->IsDead()) {
                std::cout << "[MONSTER MANAGER] Removing dead monster " 
                          << (*it)->GetId() << std::endl;
                delete *it;
                it = monsters.erase(it);
            } else {
                ++it;
            }
        }
    }

    const std::vector<Monster*>& MonsterManager::GetAllMonsters() const {
        return monsters;
    }

    Monster* MonsterManager::FindMonsterAt(float x, float y, float range) {
        for (auto monster : monsters) {
            if (monster->IsDead()) continue;
            
            float dx = monster->GetPosition().x - x;
            float dy = monster->GetPosition().y - y;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance <= range) {
                return monster;
            }
        }
        return nullptr;
    }
}