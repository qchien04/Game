#pragma once

namespace AnCom {
    struct Position {
        float x;
        float y;
    };

    class Entity {
    public:
        Entity(float x, float y, float height, float width, float speed);
        ~Entity();

        const Position& GetPosition() const;
        void SetPosition(float x, float y);

        virtual void Move(float dx, float dy);

        virtual void Move(bool isLeft, bool isRight, bool isUp, bool isDown);

        float GetDistanceTo(float x, float y);

        float GetDistanceTo(Entity& entity);
    protected:
        Position m_position;

        float m_height;
        float m_width;

        float m_speed;
    };
}
