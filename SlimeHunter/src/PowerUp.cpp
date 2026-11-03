#include "PowerUp.hpp"
#include "Player.hpp"
#include "Utils.hpp"
#include "Weapon.hpp"

#include <algorithm>

std::vector<PowerUpCard> PowerUpManager::getRandomCards(int count) {
    std::vector<PowerUpCard> pool = {
        {PowerUpType::AttackSpeed,    "Gậy Thần Tốc",       "+25% Tốc độ ra đòn tấn công",            "[SPD]"},
        {PowerUpType::CriticalStrike, "Chí Mạng Chí Mạng",  "+15% Tỉ lệ đòn đánh x2 sát thương",      "[CRT]"},
        {PowerUpType::BurnAura,       "Hào Quang Thiêu Đốt", "Đốt cháy quái vật đứng sát người chơi", "[BRN]"},
        {PowerUpType::SwiftBoots,     "Giày Lông Vũ",       "+15% Tốc độ di chuyển cơ bản",           "[BOT]"},
        {PowerUpType::SuperMagnet,    "Nam Châm Cực Đại",   "+150px Bán kính tự động hút Exp & Vàng", "[MAG]"},
        {PowerUpType::DragonBlood,    "Máu Rồng Tối Thượng","+35 Máu tối đa và hồi đầy bình máu",     "[HP+]"},
        {PowerUpType::WeaponWand,     "Đổi Gậy Phép Thuật", "Bắn cầu phép tầm xa xuyên quái",         "[WND]"},
        {PowerUpType::WeaponBoomerang,"Đổi Boomerang",      "Ném boomerang xoay tít bay 2 chiều",     "[BMG]"}
    };

    std::shuffle(pool.begin(), pool.end(), util::rng());
    if (pool.size() > static_cast<std::size_t>(count)) {
        pool.resize(count);
    }
    return pool;
}

void PowerUpManager::applyCard(Player& player, const PowerUpCard& card) {
    switch (card.type) {
        case PowerUpType::AttackSpeed:
            player.addAttackSpeedBonus(0.25f);
            break;
        case PowerUpType::CriticalStrike:
            player.addCritChance(0.15f);
            break;
        case PowerUpType::BurnAura:
            player.setBurnAura(true);
            break;
        case PowerUpType::SwiftBoots:
            player.setSpeed(player.getSpeed() * 1.15f);
            break;
        case PowerUpType::SuperMagnet:
            player.setMagnetRadius(player.getMagnetRadius() + 150.f);
            break;
        case PowerUpType::DragonBlood:
            player.setMaxHp(player.getMaxHp() + 35.f);
            player.heal(1000.f);
            break;
        case PowerUpType::WeaponWand:
            player.setWeapon(std::make_unique<MagicWand>());
            break;
        case PowerUpType::WeaponBoomerang:
            player.setWeapon(std::make_unique<BoomerangWeapon>());
            break;
    }
}
