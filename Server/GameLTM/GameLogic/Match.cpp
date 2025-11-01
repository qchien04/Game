#include "Match.h"
#include "BulletManager.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <winsock2.h>   // ntohl, htons, ...
#pragma comment(lib, "ws2_32.lib")


namespace AnCom {

    std::atomic<int> Match::next_match_id(1);

    void Match::EnqueuePlayerAction(const PlayerAction& action) {
        action_queue.enqueue(action);
    }
    
    void Match::PlayerAttack(int player_id, float target_x, float target_y) {
        for (auto player : players) {
            if (player->GetId() == player_id) {
                float damage = player->GetMaxDamage();
                Bullet* bullet = new Bullet(player->GetPosition().x, player->GetPosition().y,
                                            50, 50, target_x, target_y,
                                            12, 1500);
                bullet_manager->AddBullet(bullet);
                break;
            }
        }
    }
    
    Match::Match() {
        this->bullet_manager = new BulletManager();
        match_id = next_match_id.load();
        Match::next_match_id.fetch_add(1);
        std::atomic_store(&snap_shot_state, std::make_shared<GameState>());
        last_update = std::chrono::steady_clock::now();
        last_broadcast = std::chrono::steady_clock::now();
    }
    
    std::ostream& operator<<(std::ostream& os, const GameState& p) {
        os << "-------------------------------------------------------------" << std::endl;
        os << "Players quantity: " << p.players.size() << std::endl;
        for (const auto& player : p.players) {
            os << "  Player ID: " << player.id << std::endl;
            os << "  Position: (" << player.x << ", " << player.y << ")" << std::endl;
        }
        os << "Bullets quantity: " << p.bullets.size() << std::endl;
        for (const auto& bullet : p.bullets) {
            os << "  Bullet Position: (" << bullet.x << ", " << bullet.y << ")" << std::endl;
        }
        os << "-------------------------------------------------------------" << std::endl;
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const PlayerAction& p) {
        os << "  Player ID: " << p.player_id << std::endl;
        os << "  (Player l: " << p.l << std::endl;
        os << "  Player r: " << p.r << std::endl;
        os << "  Player u: " << p.u << std::endl;
        os << "  Player d: " << p.d << std::endl;
        os << "  Player action: " << p.action << std::endl;
        os << "  Player targetX: " << p.target_x << std::endl;
        os << "  Player targetY: " << p.target_y <<")"<< std::endl;
        return os;
    }

    Match::~Match() {
        delete bullet_manager; // ✅ tránh rò rỉ bộ nhớ
    }

    unsigned int Match::GetMatchId() const { return match_id; }

    void Match::AddPlayer(Player* player) {
        players.push_back(player);
    }

    void Match::RemovePlayer(Player* player) {
        // Optional: Implement if needed
    }

    void Match::GetMatchState() {}

    // void Match::Update(){
        
    //     for(auto player : players){
    //         player->Move(false, true, false, false);
    //     }
    //     bullet_manager->Update();
        
    //     GameState gameState;
    //     gameState.match_id = match_id;
    //     for(auto player : players){
    //         PlayerState playerState;
    //         playerState.id = player->GetId();
    //         playerState.x = player->GetPosition().x;
    //         playerState.y = player->GetPosition().y;
    //         gameState.players.push_back(playerState);
    //     }
    //     for(auto bullet : bullet_manager->GetAllBullet()){
    //         BulletState bulletState;
    //         bulletState.x = bullet->GetPosition().x;
    //         bulletState.y = bullet->GetPosition().y;
    //         gameState.bullets.push_back(bulletState);
    //     }
    //     std::shared_ptr<GameState> new_state = std::make_shared<GameState>(gameState);
    //     std::atomic_store(&snap_shot_state, new_state);
    // }

    void Match::Update() {
        constexpr size_t MAX_ACTIONS_PER_FRAME = 128;
        std::vector<PlayerAction> actions(MAX_ACTIONS_PER_FRAME);

        size_t count = action_queue.try_dequeue_bulk(actions.begin(), MAX_ACTIONS_PER_FRAME);

        // 2. Xử lý các hành động vừa lấy ra
        for (size_t i = 0; i < count; ++i) {
            const PlayerAction& action = actions[i];
            for (size_t i = 0; i < players.size(); ++i) {
                if(players[i]->GetId()==action.player_id){
                    players[i]->Move(action.l,action.r,action.u,action.d);

                    if (action.action == 1) {
                        float damage = players[i]->GetMaxDamage();
                        Bullet* bullet = new Bullet(players[i]->GetPosition().x, players[i]->GetPosition().y,
                                                    50, 50, action.target_x, action.target_y,
                                                    12, 1500);
                        bullet_manager->AddBullet(bullet);
                    }
                    break;
                }
            }
        }

        bullet_manager->Update();
        
        // 4. Tạo snapshot trạng thái game
        GameState gameState;
        gameState.match_id = match_id;

        for (auto player : players) {
            PlayerState playerState;
            playerState.id = player->GetId();
            playerState.x = player->GetPosition().x;
            playerState.y = player->GetPosition().y;
            playerState.health = player->GetCurrentHealth();
            gameState.players.push_back(playerState);
        }

        for (auto bullet : bullet_manager->GetAllBullet()) {
            BulletState bulletState;
            bulletState.x = bullet->GetPosition().x;
            bulletState.y = bullet->GetPosition().y;
            gameState.bullets.push_back(bulletState);
        }

        // 5. Cập nhật snapshot
        // std::cout<<"[UPDATE] ";
        // std::cout<<gameState<<std::endl;
        std::shared_ptr<GameState> new_state = std::make_shared<GameState>(gameState);
        std::atomic_store(&snap_shot_state, new_state);
    }


    void Match::AddVirtualPlayer(int id) {
        Player* v_player = new Player(1, 1, 3, 3, 6, 1, 3, 10, 10, 1, id);
        this->AddPlayer(v_player);
    }


    void Match::Debug() {
        this->bullet_manager->Debug();
    }

    GameState Match::GetGameState() {
        std::shared_ptr<GameState> snapshot = std::atomic_load(&snap_shot_state);
        if (snapshot) {
            return *snapshot;
        } else {
            return GameState();
        }
    }

    PlayerAction Match::DePayload(int player_id,const uint8_t* payload, size_t size) {
        PlayerAction action;
            int32_t x = 0;
            int32_t y = 0;
            int32_t action_type = 0;
            int32_t action_direction=0;
            int32_t target_x = 0;
            int32_t target_y = 0;
        
        if (size < 4 * sizeof(int)) {
            std::cout << "Payload quá nhỏ để đọc l, r, u, d\n";

            return action;
        }

        int l_raw, r_raw, u_raw, d_raw, action_type_raw, action_direction_raw, target_x_raw, target_y_raw;

        std::memcpy(&l_raw, payload + 0, sizeof(int));
        std::memcpy(&r_raw, payload + 4, sizeof(int));
        std::memcpy(&u_raw, payload + 8, sizeof(int));
        std::memcpy(&d_raw, payload + 12, sizeof(int));
        std::memcpy(&action_type_raw, payload + 16, sizeof(int));
        std::memcpy(&action_direction_raw, payload + 20, sizeof(int));
        std::memcpy(&target_x_raw, payload + 24, sizeof(int));
        std::memcpy(&target_y_raw, payload + 28, sizeof(int));

        action.player_id=player_id;
        action.l = ntohl(l_raw);
        action.r = ntohl(r_raw);
        action.u  = ntohl(u_raw);
        action.d = ntohl(d_raw);
        action.action = ntohl(action_type_raw);
        action.target_x  = ntohl(target_x_raw);
        action.target_y = ntohl(target_y_raw);    

        // std::cout<<action<<std::endl;
        return action;
    }

    uint32_t htonf(float f) {
        uint32_t temp;
        std::memcpy(&temp, &f, sizeof(float));
        return htonl(temp);
    }

    void Match::SerializeGameState(uint8_t* payload, size_t& size) {
        std::cout<<"[BROADCAST] ";
        std::cout <<  "Match ID: "<<this->match_id<<std::endl;
        std::cout<<*this->snap_shot_state;
        size_t offset = 0;

        int32_t match_id_net = htonl(this->snap_shot_state->match_id);
        std::memcpy(payload + offset, &match_id_net, sizeof(int32_t));
        offset += sizeof(int32_t);

        int32_t player_count = htonl(static_cast<int32_t>(this->snap_shot_state->players.size()));
        std::memcpy(payload + offset, &player_count, sizeof(int32_t));
        offset += sizeof(int32_t);

        for (const auto& p : this->snap_shot_state->players) {
            int32_t id = htonl(p.id);
            uint32_t x = htonf(p.x);
            uint32_t y = htonf(p.y);
            int32_t health = htonl(p.health);

            std::memcpy(payload + offset, &id, sizeof(int32_t)); offset += sizeof(int32_t);
            std::memcpy(payload + offset, &x, sizeof(float));    offset += sizeof(float);
            std::memcpy(payload + offset, &y, sizeof(float));    offset += sizeof(float);
            std::memcpy(payload + offset, &health, sizeof(int32_t)); offset += sizeof(int32_t);
        }

        int32_t bullet_count = htonl(static_cast<int32_t>(this->snap_shot_state->bullets.size()));
        std::memcpy(payload + offset, &bullet_count, sizeof(int32_t));
        offset += sizeof(int32_t);

        for (const auto& b : this->snap_shot_state->bullets) {
            uint32_t x = htonf(b.x);
            uint32_t y = htonf(b.y);
            std::memcpy(payload + offset, &x, sizeof(float)); offset += sizeof(float);
            std::memcpy(payload + offset, &y, sizeof(float)); offset += sizeof(float);
        }
        size = offset;
    }

    const std::vector<Player*>& AnCom::Match::GetAllPlayer() const {
        return this->players;
    }
}
