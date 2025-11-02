#include "Match.h"
#include "BulletManager.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <winsock2.h>
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
        this->monster_manager = new MonsterManager();
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
            os << "  Health: " << player.health << std::endl;
        }
        os << "Bullets quantity: " << p.bullets.size() << std::endl;
        for (const auto& bullet : p.bullets) {
            os << "  Bullet Position: (" << bullet.x << ", " << bullet.y << ")" << std::endl;
        }
        os << "-------------------------------------------------------------" << std::endl;

        os << "Slime quantity: " << p.monsters.size() << std::endl;
        for (const auto& monster : p.monsters) {
            os << "  Bullet Position: (" << monster.x << ", " << monster.y << ")" << std::endl;
            os << "  Health: " << monster.health << std::endl;
            os << "  State: " << monster.state << std::endl;        
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
        delete bullet_manager;
        delete monster_manager;
    }

    unsigned int Match::GetMatchId() const { 
        return match_id; 
    }

    void Match::AddPlayer(Player* player) {
        players.push_back(player);
    }

    void Match::RemovePlayer(Player* player) {
        players.erase(std::remove(players.begin(), players.end(), player), players.end());
    }

    void Match::GetMatchState() {}

    // ==================== COLLISION DETECTION ====================
    
    bool Match::CheckCollision(float x1, float y1, float w1, float h1,
                               float x2, float y2, float w2, float h2) {
        return !(x1 + w1 < x2 || x1 > x2 + w2 || 
                 y1 + h1 < y2 || y1 > y2 + h2);
    }

    float Match::CalculateDistance(float x1, float y1, float x2, float y2) {
        float dx = x2 - x1;
        float dy = y2 - y1;
        return std::sqrt(dx * dx + dy * dy);
    }

    void Match::SpawnSlime(float x, float y, int id) {
        Monster* slime = new Monster(x, y, 32, 32, 50.0f, 100, id, 1);
        monster_manager->AddMonster(slime);
        std::cout << "[MATCH] Spawned Slime #" << id << " at (" 
                << x << ", " << y << ")" << std::endl;
    }

    void Match::CheckBulletPlayerCollisions() {
        auto& bullets = bullet_manager->GetAllBullet();
        
        for (auto bullet_it = bullets.begin(); bullet_it != bullets.end();) {
            Bullet* bullet = *bullet_it;
            bool bullet_hit = false;
            
            for (auto player : players) {
                if (player->IsDie()) continue;
                
                // Kiểm tra va chạm giữa bullet và player
                if (CheckCollision(
                    bullet->GetPosition().x, bullet->GetPosition().y, 50, 50,
                    player->GetPosition().x, player->GetPosition().y, 3, 3)) {
                    
                    // Giảm máu player
                    int current_health = player->GetCurrentHealth();
                    int new_health = current_health - 10; // Damage từ bullet
                    player->SetCurrentHealth(new_health);
                    
                    std::cout << "[BULLET HIT] Player " << player->GetId() 
                              << " hit by bullet! Health: " << current_health 
                              << " -> " << new_health << std::endl;
                    
                    if (player->IsDie()) {
                        std::cout << "[DEATH] Player " << player->GetId() 
                                  << " has died from bullet!" << std::endl;
                    }
                    
                    bullet_hit = true;
                    break;
                }
            }
            
            if (bullet_hit) {
                delete *bullet_it;
                bullet_it = const_cast<std::vector<Bullet*>&>(bullets).erase(bullet_it);
            } else {
                ++bullet_it;
            }
        }
    }

    void Match::CheckMeleeAttack(int attacker_id, float target_x, float target_y) {
        Player* attacker = nullptr;
        
        // Tìm player tấn công
        for (auto player : players) {
            if (player->GetId() == attacker_id && !player->IsDie()) {
                attacker = player;
                break;
            }
        }
        
        if (!attacker) return;
        
        const float MELEE_RANGE = 100.0f; // Tầm đánh cận chiến
        const float MELEE_WIDTH = 80.0f;  // Độ rộng của vùng tấn công
        
        float attacker_x = attacker->GetPosition().x;
        float attacker_y = attacker->GetPosition().y;
        
        // Tính hướng tấn công
        float dx = target_x - attacker_x;
        float dy = target_y - attacker_y;
        float angle = std::atan2(dy, dx);
        
        std::cout << "[MELEE ATTACK] Player " << attacker_id 
                  << " swings at (" << target_x << ", " << target_y 
                  << ") angle: " << (angle * 180.0f / 3.14159f) << "°" << std::endl;
        
        // Kiểm tra tất cả player trong tầm
        for (auto victim : players) {
            if (victim->GetId() == attacker_id || victim->IsDie()) continue;
            
            float victim_x = victim->GetPosition().x;
            float victim_y = victim->GetPosition().y;
            
            // Tính khoảng cách từ attacker đến victim
            float distance = CalculateDistance(attacker_x, attacker_y, victim_x, victim_y);
            
            if (distance > MELEE_RANGE) continue;
            
            // Tính góc từ attacker đến victim
            float victim_angle = std::atan2(victim_y - attacker_y, victim_x - attacker_x);
            float angle_diff = std::abs(victim_angle - angle);
            
            // Normalize angle difference to [0, PI]
            while (angle_diff > 3.14159f) angle_diff -= 2 * 3.14159f;
            angle_diff = std::abs(angle_diff);
            
            // Kiểm tra nếu victim nằm trong góc tấn công (khoảng 60 độ)
            const float ATTACK_ANGLE = 0.524f; // 30 độ mỗi bên = 60 độ tổng
            
            if (angle_diff <= ATTACK_ANGLE) {
                // Victim bị trúng đòn!
                int current_health = victim->GetCurrentHealth();
                int melee_damage = static_cast<int>(attacker->GetMaxDamage() * 1.5f); // Melee mạnh hơn 50%
                int new_health = current_health - melee_damage;
                victim->SetCurrentHealth(new_health);
                
                std::cout << "[MELEE HIT] Player " << victim->GetId() 
                          << " hit by Player " << attacker_id 
                          << "! Damage: " << melee_damage
                          << ", Health: " << current_health 
                          << " -> " << new_health 
                          << ", Distance: " << distance << std::endl;
                
                if (victim->IsDie()) {
                    std::cout << "[DEATH] Player " << victim->GetId() 
                              << " has been slain by Player " << attacker_id << "!" << std::endl;
                }
                
                // Đẩy lùi victim (knockback effect)
                float knockback_force = 15.0f;
                float push_x = std::cos(angle) * knockback_force;
                float push_y = std::sin(angle) * knockback_force;
                victim->Move(push_x, push_y);
            }
        }
    }

    void Match::CheckPlayerPlayerCollisions() {
        for (size_t i = 0; i < players.size(); ++i) {
            for (size_t j = i + 1; j < players.size(); ++j) {
                if (players[i]->IsDie() || players[j]->IsDie()) continue;
                
                // Kiểm tra va chạm giữa 2 player
                if (CheckCollision(
                    players[i]->GetPosition().x, players[i]->GetPosition().y, 3, 3,
                    players[j]->GetPosition().x, players[j]->GetPosition().y, 3, 3)) {
                    
                    // Đẩy lùi các player ra khỏi nhau
                    float dx = players[j]->GetPosition().x - players[i]->GetPosition().x;
                    float dy = players[j]->GetPosition().y - players[i]->GetPosition().y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    if (distance > 0) {
                        float overlap = (3 + 3) - distance;
                        float push_x = (dx / distance) * overlap * 0.5f;
                        float push_y = (dy / distance) * overlap * 0.5f;
                        
                        players[i]->Move(-push_x, -push_y);
                        players[j]->Move(push_x, push_y);
                    }
                }
            }
        }
    }

    void Match::RemoveDeadPlayers() {
        for (auto it = players.begin(); it != players.end();) {
            if ((*it)->IsDie()) {
                std::cout << "[REMOVE] Removing dead player " << (*it)->GetId() << std::endl;
                delete *it;
                it = players.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ==================== UPDATE LOOP ====================

    void Match::Update() {
        constexpr size_t MAX_ACTIONS_PER_FRAME = 128;
        std::vector<PlayerAction> actions(MAX_ACTIONS_PER_FRAME);

        // 1. Lấy các action từ queue
        size_t count = action_queue.try_dequeue_bulk(actions.begin(), MAX_ACTIONS_PER_FRAME);

        // 2. Xử lý các hành động
        for (size_t i = 0; i < count; ++i) {
            const PlayerAction& action = actions[i];
            
            for (auto player : players) {
                if (player->GetId() == action.player_id && !player->IsDie()) {
                    // Di chuyển player
                    player->Move(action.l, action.r, action.u, action.d);

                    // Xử lý attack
                    if (action.action == 1) {
                        // Bắn đạn (ranged attack)
                        float damage = player->GetMaxDamage();
                        Bullet* bullet = new Bullet(
                            player->GetPosition().x, 
                            player->GetPosition().y,
                            50, 50, 
                            action.target_x, 
                            action.target_y,
                            12, 1500
                        );
                        bullet_manager->AddBullet(bullet);
                        
                        std::cout << "[RANGED ATTACK] Player " << player->GetId() 
                                  << " fired bullet to (" << action.target_x 
                                  << ", " << action.target_y << ")" << std::endl;
                    }
                    else if (action.action == 2) {
                        // Chém (melee attack)
                        CheckMeleeAttack(action.player_id, action.target_x, action.target_y);
                    }
                    break;
                }
            }
        }

        // 3. Cập nhật bullets
        bullet_manager->Update();
        

        // ========== PHASE 2.5: Update Monsters ==========
        auto now = std::chrono::steady_clock::now();
        float delta_time = std::chrono::duration<float>(now - last_update).count();
        last_update = now;
        
        monster_manager->Update(delta_time);
        
        // Check bullet-monster collision
        auto& bullets = bullet_manager->GetAllBullet();
        for (auto bullet_it = bullets.begin(); bullet_it != bullets.end();) {
            Bullet* bullet = *bullet_it;
            bool hit = false;
            
            for (auto monster : monster_manager->GetAllMonsters()) {
                if (monster->IsDead()) continue;
                
                if (CheckCollision(
                    bullet->GetPosition().x, bullet->GetPosition().y, 50, 50,
                    monster->GetPosition().x, monster->GetPosition().y, 32, 32)) {
                    
                    monster->TakeDamage(10);
                    hit = true;
                    break;
                }
            }
            
            if (hit) {
                delete *bullet_it;
                bullet_it = const_cast<std::vector<Bullet*>&>(bullets).erase(bullet_it);
            } else {
                ++bullet_it;
            }
        }
        
        // Remove dead monsters
        monster_manager->RemoveDeadMonsters();
        // 4. Kiểm tra va chạm
        CheckBulletPlayerCollisions();
        CheckPlayerPlayerCollisions();

        // 5. Xóa player đã chết
        RemoveDeadPlayers();
        
        // 6. Tạo snapshot trạng thái game
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

        for (auto monster : monster_manager->GetAllMonsters()) {
            MonsterState monsterState;
            monsterState.id = monster->GetId();
            monsterState.type = monster->GetMonsterType();
            monsterState.x = monster->GetPosition().x;
            monsterState.y = monster->GetPosition().y;
            monsterState.health = monster->GetHealth();
            monsterState.state = static_cast<int>(monster->GetState());
            monsterState.direction = static_cast<int>(monster->GetDirection());
            gameState.monsters.push_back(monsterState);
        }


        // 7. Cập nhật snapshot
        std::shared_ptr<GameState> new_state = std::make_shared<GameState>(gameState);
        std::atomic_store(&snap_shot_state, new_state);
        //std::cout<<gameState<<std::endl;
    }

    // ==================== UTILITY FUNCTIONS ====================

    void Match::AddVirtualPlayer(int id) {
        Player* v_player = new Player(1, 1, 3, 3, 6, 1, 3, 100000, 10, 1, id);
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

    PlayerAction Match::DePayload(int player_id, const uint8_t* payload, size_t size) {
        PlayerAction action;
        
        if (size < 8 * sizeof(int)) {
            std::cout << "Payload quá nhỏ để đọc dữ liệu\n";
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

        action.player_id = player_id;
        action.l = ntohl(l_raw);
        action.r = ntohl(r_raw);
        action.u = ntohl(u_raw);
        action.d = ntohl(d_raw);
        action.action = ntohl(action_type_raw);
        action.target_x = ntohl(target_x_raw);
        action.target_y = ntohl(target_y_raw);

        return action;
    }

    uint32_t htonf(float f) {
        uint32_t temp;
        std::memcpy(&temp, &f, sizeof(float));
        return htonl(temp);
    }

    // void Match::SerializeGameStat2e(uint8_t* payload, size_t& size) {
    //     std::cout << "[BROADCAST] Match ID: " << this->match_id << std::endl;
    //     std::cout << *this->snap_shot_state;
        
    //     size_t offset = 0;

    //     int32_t match_id_net = htonl(this->snap_shot_state->match_id);
    //     std::memcpy(payload + offset, &match_id_net, sizeof(int32_t));
    //     offset += sizeof(int32_t);

    //     int32_t player_count = htonl(static_cast<int32_t>(this->snap_shot_state->players.size()));
    //     std::memcpy(payload + offset, &player_count, sizeof(int32_t));
    //     offset += sizeof(int32_t);

    //     for (const auto& p : this->snap_shot_state->players) {
    //         int32_t id = htonl(p.id);
    //         uint32_t x = htonf(p.x);
    //         uint32_t y = htonf(p.y);
    //         int32_t health = htonl(p.health);

    //         std::memcpy(payload + offset, &id, sizeof(int32_t)); 
    //         offset += sizeof(int32_t);
    //         std::memcpy(payload + offset, &x, sizeof(float));    
    //         offset += sizeof(float);
    //         std::memcpy(payload + offset, &y, sizeof(float));    
    //         offset += sizeof(float);
    //         std::memcpy(payload + offset, &health, sizeof(int32_t)); 
    //         offset += sizeof(int32_t);
    //     }

    //     int32_t bullet_count = htonl(static_cast<int32_t>(this->snap_shot_state->bullets.size()));
    //     std::memcpy(payload + offset, &bullet_count, sizeof(int32_t));
    //     offset += sizeof(int32_t);

    //     for (const auto& b : this->snap_shot_state->bullets) {
    //         uint32_t x = htonf(b.x);
    //         uint32_t y = htonf(b.y);
    //         std::memcpy(payload + offset, &x, sizeof(float)); 
    //         offset += sizeof(float);
    //         std::memcpy(payload + offset, &y, sizeof(float)); 
    //         offset += sizeof(float);
    //     }
        
    //     size = offset;
    // }

    void Match::SerializeGameState(uint8_t* payload, size_t& size) {
        size_t offset = 0;

        // Match ID
        int32_t match_id_net = htonl(this->snap_shot_state->match_id);
        std::memcpy(payload + offset, &match_id_net, sizeof(int32_t));
        offset += sizeof(int32_t);

        // Players
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

        // Bullets
        int32_t bullet_count = htonl(static_cast<int32_t>(this->snap_shot_state->bullets.size()));
        std::memcpy(payload + offset, &bullet_count, sizeof(int32_t));
        offset += sizeof(int32_t);

        for (const auto& b : this->snap_shot_state->bullets) {
            uint32_t x = htonf(b.x);
            uint32_t y = htonf(b.y);
            std::memcpy(payload + offset, &x, sizeof(float)); offset += sizeof(float);
            std::memcpy(payload + offset, &y, sizeof(float)); offset += sizeof(float);
        }
        
        // Monsters ← MỚI THÊM
        int32_t monster_count = htonl(static_cast<int32_t>(this->snap_shot_state->monsters.size()));
        std::memcpy(payload + offset, &monster_count, sizeof(int32_t));
        offset += sizeof(int32_t);

        for (const auto& m : this->snap_shot_state->monsters) {
            int32_t id = htonl(m.id);
            int32_t type = htonl(m.type);
            uint32_t x = htonf(m.x);
            uint32_t y = htonf(m.y);
            int32_t health = htonl(m.health);
            int32_t state = htonl(m.state);
            int32_t direction = htonl(m.direction);

            std::memcpy(payload + offset, &id, sizeof(int32_t));        offset += sizeof(int32_t);
            std::memcpy(payload + offset, &type, sizeof(int32_t));      offset += sizeof(int32_t);
            std::memcpy(payload + offset, &x, sizeof(float));           offset += sizeof(float);
            std::memcpy(payload + offset, &y, sizeof(float));           offset += sizeof(float);
            std::memcpy(payload + offset, &health, sizeof(int32_t));    offset += sizeof(int32_t);
            std::memcpy(payload + offset, &state, sizeof(int32_t));     offset += sizeof(int32_t);
            std::memcpy(payload + offset, &direction, sizeof(int32_t)); offset += sizeof(int32_t);
        }
        
        size = offset;
        
        std::cout << "[BROADCAST] Match " << this->match_id 
                << " | Players: " << this->snap_shot_state->players.size()
                << " | Bullets: " << this->snap_shot_state->bullets.size()
                << " | Monsters: " << this->snap_shot_state->monsters.size() << std::endl;
    }

    const std::vector<Player*>& Match::GetAllPlayer() const {
        return this->players;
    }
}