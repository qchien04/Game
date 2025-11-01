#pragma once
#include "Entity.h"

namespace AnCom{
    struct Vector2{
        float x;
        float y;
    };

    class Bullet:public Entity{
        public:
            Bullet(float x, float y,float height, float width, 
                    float target_x, float target_y, 
                    float speed, float MaxFlightDistance);

            ~Bullet();

            bool Update();

            void Debug();
        
        private:
            float src_x;
            float src_y;    

            float maxFlightDistance;

            float target_x;
            float target_y;

            Vector2 velocity;

    };
}
