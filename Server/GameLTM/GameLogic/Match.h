#pragma once
#include "Player.h"
#include "BulletManager.h"
#include <vector>
#include <ostream>
#include <memory>
#include <atomic>
#include <algorithm>
#include "concurrentqueue.h"
#include "Monster/MonsterManager.h"

namespace AnCom {
    struct PlayerState {
        int id;
        float x;
        float y;
        int health;
    };

    struct PlayerAction {
        int player_id;
        bool l, r, u, d;
        int action;
        int target_x, target_y;
    };

    struct BulletState {
        float x;
        float y;
    };
    struct MonsterState {
        int id;
        int type;
        float x;
        float y;
        int health;
        int state;      // 0=IDLE, 1=RUN, 2=ATTACK, 3=DEAD
        int direction;  // 0=NONE, 1=UP, 2=DOWN, 3=LEFT, 4=RIGHT
    };

    struct GameState {
        int match_id;
        std::vector<PlayerState> players;
        std::vector<BulletState> bullets; 
        std::vector<MonsterState> monsters;
    };

    std::ostream& operator<<(std::ostream& os, const GameState& state);
    std::ostream& operator<<(std::ostream& os, const PlayerAction& action);
    
    class Match {
        static std::atomic<int> next_match_id;
        
    public:
        Match();
        ~Match();

        std::chrono::time_point<std::chrono::steady_clock> last_update;
        std::chrono::time_point<std::chrono::steady_clock> last_broadcast;

        void SerializeGameState(uint8_t* payload, size_t& size);

        unsigned int GetMatchId() const;

        void AddPlayer(Player* player);
        void RemovePlayer(Player* player);

        void GetMatchState();

        void PlayerAttack(int player_id, float target_x, float target_y);
        void PlayerMove(int player_id, float x, float y);

        void Update();

        const std::vector<Player*>& GetAllPlayer() const;

        void AddVirtualPlayer(int id);

        static PlayerAction DePayload(int player_id, const uint8_t* payload, size_t size);

        void Debug();

        GameState GetGameState();

        void EnqueuePlayerAction(const PlayerAction& action);
        
        // ===== Collision Detection Functions =====
        bool CheckCollision(float x1, float y1, float w1, float h1,
                           float x2, float y2, float w2, float h2);
        
        void CheckBulletPlayerCollisions();
        void CheckPlayerPlayerCollisions();
        void CheckMeleeAttack(int attacker_id, float target_x, float target_y);
        float CalculateDistance(float x1, float y1, float x2, float y2);
        void RemoveDeadPlayers();

        void SpawnSlime(float x, float y, int id);
        
    private:
        unsigned int match_id;
        std::shared_ptr<GameState> snap_shot_state;

        std::vector<Player*> players;

        BulletManager* bullet_manager;

        MonsterManager* monster_manager;

        moodycamel::ConcurrentQueue<PlayerAction> action_queue;
    };
}