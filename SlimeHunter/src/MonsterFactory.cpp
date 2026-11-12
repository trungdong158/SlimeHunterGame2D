#include "MonsterFactory.hpp"
#include "Assets.hpp"
#include "Bat.hpp"
#include "Skeleton.hpp"
#include "Slime.hpp"

std::unique_ptr<Monster> MonsterFactory::createMonster(MonsterType type, sf::Vector2f pos,
                                                     const balance::MonsterScale& scale,
                                                     const Assets& assets) {
    switch (type) {
        case MonsterType::SlimeGreen:
            return std::make_unique<Slime>(assets, SlimeType::Green, pos, scale);
        case MonsterType::SlimePurple:
            return std::make_unique<Slime>(assets, SlimeType::Purple, pos, scale);
        case MonsterType::SlimeTank:
            return std::make_unique<Slime>(assets, SlimeType::Tank, pos, scale);
        case MonsterType::SlimeKing:
            return std::make_unique<Slime>(assets, SlimeType::King, pos, scale);
        case MonsterType::Skeleton:
            return std::make_unique<Skeleton>(assets, pos, scale);
        case MonsterType::Bat:
            return std::make_unique<Bat>(assets, pos, scale);
    }
    return std::make_unique<Slime>(assets, SlimeType::Green, pos, scale);
}
