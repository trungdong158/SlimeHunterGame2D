#include "Skeleton.hpp"
#include "Assets.hpp"
#include "Utils.hpp"

#include <cmath>

Skeleton::Skeleton(const Assets& assets, sf::Vector2f startPosition, const balance::MonsterScale& scale)
    : Monster(pickTexture(assets), assets.shadow, makeStats(scale), makeVisual(), startPosition) {
}

void Skeleton::performRangedAttack(std::vector<Projectile>& projectiles, const sf::Vector2f& playerPos) {
    const sf::Vector2f dir = util::normalized(playerPos - m_position);
    const sf::Vector2f spawnPos = m_position + sf::Vector2f(0.f, -25.f);
    // Bắn mũi tên bay thẳng về phía Player
    projectiles.emplace_back(spawnPos, dir * 340.f, m_stats.damage, 2.2f, 7.f, false, ProjectileType::Arrow, sf::Color(230, 230, 230));
}

MonsterStats Skeleton::makeStats(const balance::MonsterScale& scale) {
    MonsterStats s;
    s.maxHp = std::round(35.f * scale.hp);
    s.damage = std::round(10.f * scale.damage);
    s.speed = 100.f * scale.speed;
    s.detectRange = 900.f;
    s.attackRange = 400.f;          // Tầm bắn xa
    s.attackCooldown = 1.8f * scale.cooldown;
    s.attackDuration = 0.55f;
    s.attackHitTime = 0.30f;
    s.footRadius = 14.f;
    s.bodySize = {44.f, 52.f};
    s.knockbackSpeed = 340.f;
    s.stunTime = 0.2f;
    s.scoreValue = 18;
    s.expValue = 16;
    return s;
}

MonsterVisual Skeleton::makeVisual() {
    MonsterVisual v;
    v.frameSize = {48, 48};
    v.origin = {24.f, 44.f};
    v.scale = 2.6f;
    v.shadowScale = 2.6f;
    v.hpBarHeight = 92.f;

    v.idle   = {0, 4, 0.18f};
    v.move   = {1, 4, 0.13f};
    v.attack = {2, 4, 0.12f};
    v.hurt   = {3, 2, 0.12f};
    v.death  = {4, 4, 0.14f};
    return v;
}

const sf::Texture& Skeleton::pickTexture(const Assets& assets) {
    return assets.skeletonSheet;
}
