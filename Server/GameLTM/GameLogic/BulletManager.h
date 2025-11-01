#pragma once

#include "Bullet.h"
#include <vector>

namespace AnCom {
    class BulletManager {
    public:
        BulletManager();
        ~BulletManager();
        
        void AddBullet(Bullet* bullet);
        void RemoveBullet(Bullet* bullet);

        const std::vector<Bullet*>& GetAllBullet() const;

        void Debug();

        void Update();

    private:
        std::vector<Bullet*> bullets;
    };
}
