#include "Levels.hpp"

#include <algorithm>
#include <array>

namespace {

// Toạ độ spawn/exit do tools/make_levels.py tính bằng BFS trên mặt nạ (đã kiểm tra: đi được từ spawn tới exit).
const std::array<LevelConfig, kLevelCount>& table() {
    static const std::array<LevelConfig, kLevelCount> levels = [] {
        std::array<LevelConfig, kLevelCount> t;

        t[0].id = 1;
        t[0].name = "Rừng xanh";
        t[0].background = "images/backgrounds/forest.jpg";
        t[0].collision = "images/backgrounds/forest_collision.png";
        t[0].spawn = {902.f, 602.f};
        t[0].exit = {1042.f, 1148.f};
        t[0].totalMonsters = 12;  t[0].maxAlive = 6;  t[0].spawnInterval = 1.6f;

        t[1].id = 2;
        t[1].name = "Rừng ma thuật";
        t[1].background = "images/backgrounds/forest_magic.jpg";
        t[1].collision = "images/backgrounds/forest_magic_collision.png";
        t[1].spawn = {758.f, 1148.f};
        t[1].exit = {1658.f, 52.f};
        t[1].totalMonsters = 14;  t[1].maxAlive = 7;  t[1].spawnInterval = 1.5f;  t[1].purpleChance = 0.3f;
        t[1].ambient = AmbientType::Fireflies;  t[1].ambientColor = sf::Color(255, 150, 255);
        t[1].clearColor = sf::Color(28, 16, 38);

        t[2].id = 3;
        t[2].name = "Hang động pha lê";
        t[2].background = "images/backgrounds/cave_crystal.jpg";
        t[2].collision = "images/backgrounds/cave_crystal_collision.png";
        t[2].spawn = {320.f, 682.f};
        t[2].exit = {760.f, 554.f};
        t[2].spawnZone = sf::FloatRect({150.f, 330.f}, {1140.f, 560.f});
        t[2].totalMonsters = 16;  t[2].maxAlive = 7;  t[2].spawnInterval = 1.5f;
        t[2].purpleChance = 0.3f;  t[2].tankChance = 0.15f;
        t[2].ambient = AmbientType::Dust;  t[2].ambientColor = sf::Color(170, 190, 255);
        t[2].clearColor = sf::Color(4, 3, 8);

        t[3].id = 4;
        t[3].name = "Vùng đất cháy";
        t[3].background = "images/backgrounds/forest_ember.jpg";
        t[3].collision = "images/backgrounds/forest_ember_collision.png";
        t[3].spawn = {758.f, 52.f};
        t[3].exit = {1658.f, 1148.f};
        t[3].totalMonsters = 20;  t[3].maxAlive = 9;  t[3].spawnInterval = 1.3f;
        t[3].purpleChance = 0.35f;  t[3].tankChance = 0.2f;
        t[3].ambient = AmbientType::Embers;  t[3].ambientColor = sf::Color(255, 140, 50);
        t[3].clearColor = sf::Color(38, 18, 10);

        t[4].id = 5;
        t[4].name = "Hang độc";
        t[4].background = "images/backgrounds/cave_toxic.jpg";
        t[4].collision = "images/backgrounds/cave_toxic_collision.png";
        t[4].spawn = {680.f, 554.f};
        t[4].exit = {1070.f, 886.f};
        t[4].spawnZone = sf::FloatRect({150.f, 330.f}, {1140.f, 560.f});
        t[4].totalMonsters = 22;  t[4].maxAlive = 9;  t[4].spawnInterval = 1.2f;
        t[4].purpleChance = 0.3f;  t[4].tankChance = 0.3f;  t[4].bossAtEnd = true;
        t[4].ambient = AmbientType::Dust;  t[4].ambientColor = sf::Color(150, 255, 150);
        t[4].clearColor = sf::Color(3, 6, 4);

        return t;
    }();
    return levels;
}

}  // namespace

const LevelConfig& getLevelConfig(int levelId) {
    const int index = std::min(std::max(levelId, 1), kLevelCount) - 1;
    return table()[static_cast<std::size_t>(index)];
}
