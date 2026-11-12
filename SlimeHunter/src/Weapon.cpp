#include "Weapon.hpp"
#include "Effects.hpp"
#include "Player.hpp"
#include "Utils.hpp"

// ----------------------------------------------------------------------------
// BaseballBat
BaseballBat::BaseballBat() = default;

void BaseballBat::attack(Player& player, const sf::Vector2f& aimDir,
                         std::vector<Projectile>& projectiles, Effects& effects) {
    (void)projectiles;
    const balance::SlashProfile& slash = player.getSlashProfile();
    const sf::Color slashColor(slash.r, slash.g, slash.b);
    effects.addSlash(player.getPosition() + sf::Vector2f(0.f, -40.f),
                     aimDir, slash.arcRadius, slash.arcSpanDeg, slashColor);
}

sf::FloatRect BaseballBat::getHitbox(const Player& player) const {
    const balance::SlashProfile& slash = player.getSlashProfile();
    const sf::Vector2f aim = player.getAimDirection();
    const sf::Vector2f center = player.getPosition() + sf::Vector2f(0.f, -40.f) + aim * slash.reach;
    const float s = slash.size;
    return sf::FloatRect({center.x - s * 0.5f, center.y - s * 0.5f}, {s, s});
}

// ----------------------------------------------------------------------------
// MagicWand
MagicWand::MagicWand() = default;

void MagicWand::attack(Player& player, const sf::Vector2f& aimDir,
                       std::vector<Projectile>& projectiles, Effects& effects) {
    const sf::Vector2f spawnPos = player.getPosition() + sf::Vector2f(0.f, -35.f) + aimDir * 20.f;
    const float damage = player.getDamage() * getDamageMultiplier();
    projectiles.emplace_back(spawnPos, aimDir * 480.f, damage, 1.8f, 9.f, true,
                             ProjectileType::MagicOrb, sf::Color(180, 90, 255));
    effects.addSparks(spawnPos, sf::Color(200, 120, 255), 6);
}

sf::FloatRect MagicWand::getHitbox(const Player& player) const {
    (void)player;
    // MagicWand đánh bằng Projectile nên hitbox cận chiến trả về rỗng
    return sf::FloatRect({-1000.f, -1000.f}, {0.f, 0.f});
}

// ----------------------------------------------------------------------------
// BoomerangWeapon
BoomerangWeapon::BoomerangWeapon() = default;

void BoomerangWeapon::attack(Player& player, const sf::Vector2f& aimDir,
                             std::vector<Projectile>& projectiles, Effects& effects) {
    const sf::Vector2f spawnPos = player.getPosition() + sf::Vector2f(0.f, -35.f) + aimDir * 15.f;
    const float damage = player.getDamage() * getDamageMultiplier();
    projectiles.emplace_back(spawnPos, aimDir * 380.f, damage, 2.0f, 12.f, true,
                             ProjectileType::Boomerang, sf::Color(255, 190, 40));
    effects.addSparks(spawnPos, sf::Color(255, 210, 80), 8);
}

sf::FloatRect BoomerangWeapon::getHitbox(const Player& player) const {
    (void)player;
    return sf::FloatRect({-1000.f, -1000.f}, {0.f, 0.f});
}
