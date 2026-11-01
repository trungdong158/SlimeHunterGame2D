#include "Bat.hpp"
#include "Assets.hpp"
#include "Utils.hpp"

#include <cmath>

Bat::Bat(const Assets& assets, sf::Vector2f startPosition, const balance::MonsterScale& scale)
    : Monster(pickTexture(assets), assets.shadow, makeStats(scale), makeVisual(), startPosition) {
}

float Bat::visualLift() const {
    // Dơi bay: nhấc cao 24px và dập dờn theo nhịp đập cánh
    m_wingTimer += 0.016f;
    return 24.f + std::sin(m_wingTimer * 10.f) * 6.f;
}

MonsterStats Bat::makeStats(const balance::MonsterScale& scale) {
    MonsterStats s;
    s.maxHp = std::round(22.f * scale.hp);      // Máu thấp
    s.damage = std::round(7.f * scale.damage);
    s.speed = 175.f * scale.speed;             // Tốc độ bay nhanh
    s.detectRange = 850.f;
    s.attackRange = 45.f;
    s.attackCooldown = 0.9f * scale.cooldown;
    s.attackDuration = 0.35f;
    s.attackHitTime = 0.18f;
    s.footRadius = 12.f;
    s.bodySize = {40.f, 36.f};
    s.knockbackSpeed = 420.f;                  // Nhẹ nên bị đẩy lùi xa
    s.stunTime = 0.25f;
    s.scoreValue = 14;
    s.expValue = 12;
    return s;
}

MonsterVisual Bat::makeVisual() {
    MonsterVisual v;
    v.frameSize = {48, 40};
    v.origin = {24.f, 32.f};
    v.scale = 2.4f;
    v.shadowScale = 2.0f;
    v.hpBarHeight = 70.f;

    v.idle   = {0, 4, 0.12f};
    v.move   = {0, 4, 0.09f};  // Đập cánh nhanh khi di chuyển
    v.attack = {1, 4, 0.10f};
    v.hurt   = {2, 2, 0.12f};
    v.death  = {3, 4, 0.12f};
    return v;
}

const sf::Texture& Bat::pickTexture(const Assets& assets) {
    return assets.batSheet;
}
