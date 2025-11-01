#include "BulletManager.h"
#include <vector>
#include <algorithm>
#include <iostream>

namespace AnCom {

        BulletManager::BulletManager() = default;

        BulletManager::~BulletManager() {
            for (auto bullet : bullets) {
                delete bullet;
            }
            bullets.clear();
        }

        const std::vector<Bullet*>& BulletManager::GetAllBullet() const {
            return bullets;
        }

        void BulletManager::AddBullet(Bullet* bullet) {
            bullets.push_back(bullet);
        }

        void BulletManager::RemoveBullet(Bullet* bullet) {
            bullets.erase(std::remove(bullets.begin(), bullets.end(), bullet), bullets.end());
        }

        void BulletManager::Debug(){
            int i=0;
            for (auto it = bullets.begin(); it != bullets.end(); ){
                Bullet* bullet=*it;
                std::cout<<"Bullet "<<i<<": ";
                bullet->Debug();
                ++it;
                i++;
            }
        }

        void BulletManager::Update() {
            for (auto it = bullets.begin(); it != bullets.end(); ) {
                Bullet* bullet = *it;
                bool should_delete = bullet->Update();
                if (should_delete) {
                    std::cout<<"Nen xoa\n";
                    delete bullet; // hoặc bulletPool->Deallocate(bullet);
                    it = bullets.erase(it);
                } else {
                    ++it;
                }
            }
        }

}
