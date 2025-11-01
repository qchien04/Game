#pragma once
#include "Player.h"
#include "BulletManager.h"
#include <vector>
#include <ostream>
#include <memory>
#include <atomic>
#include "concurrentqueue.h"

namespace AnCom{
    struct PlayerState{
        int id;
        float x;
        float y;
        int health;
    };

    struct PlayerAction {
        int player_id;
        bool l,r,u,d;
        int action;
        int target_x,target_y;
    };

    struct BulletState{
        float x;
        float y;
    };

    struct GameState{
        int match_id;
        std::vector<PlayerState> players;
        std::vector<BulletState> bullets; 
    };

    std::ostream& operator<<(std::ostream& os, const GameState& state);
    std::ostream& operator<<(std::ostream& os, const PlayerAction& action);
    

    class Match{
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
        
        private:
            unsigned int match_id;
            std::shared_ptr<GameState> snap_shot_state;

            std::vector<Player*> players;

            BulletManager* bullet_manager;

            moodycamel::ConcurrentQueue<PlayerAction> action_queue;
            
    };
}