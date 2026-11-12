#include "Slime.hpp"

#include "Assets.hpp"

namespace {
// Hàng animation trong sprite sheet slime (xem tools/make_assets.py)
const int kRowIdle = 0;
const int kRowHop = 1;     // 6 frame: ép -> chuẩn bị -> bật lên -> đỉnh -> rơi -> tiếp đất
const int kRowAttack = 2;
const int kRowHurt = 3;
const int kRowDeath = 4;

// Frame 2..4 của cú nảy là lúc slime "bay trên không" -> mới di chuyển được.
const int kAirFirst = 2;
const int kAirLast = 4;
}  // namespace

Slime::Slime(const Assets& assets, SlimeType type, sf::Vector2f startPosition, const balance::MonsterScale& scale)
    : Monster(pickTexture(assets, type), assets.shadow, makeStats(type, scale), makeVisual(type), startPosition) {
    setBoss(type == SlimeType::King);
}

const sf::Texture& Slime::pickTexture(const Assets& assets, SlimeType type) {
    switch (type) {
        case SlimeType::Purple: return assets.slimePurple;
        case SlimeType::Tank: return assets.slimeBlue;
        case SlimeType::King: return assets.slimeKing;
        case SlimeType::Green: break;
    }
    return assets.slimeGreen;
}

MonsterStats Slime::makeStats(SlimeType type, const balance::MonsterScale& scale) {
    MonsterStats s;
    switch (type) {
        case SlimeType::Green:  // slime xanh: quái cơ bản
            s.maxHp = 40.f;  s.damage = 8.f;  s.speed = 150.f;  s.attackRange = 58.f;
            s.attackCooldown = 1.3f;  s.footRadius = 16.f;  s.bodySize = {56.f, 52.f};
            s.scoreValue = 10;  s.expValue = 12;
            break;
        case SlimeType::Purple:  // slime tím: nhanh, mỏng máu, đòn nhanh hơn
            s.maxHp = 28.f;  s.damage = 10.f;  s.speed = 215.f;  s.attackRange = 58.f;
            s.attackCooldown = 1.0f;  s.footRadius = 16.f;  s.bodySize = {56.f, 52.f};
            s.scoreValue = 25;  s.expValue = 22;
            break;
        case SlimeType::Tank:  // slime xanh dương: trâu bò, chậm
            s.maxHp = 95.f;  s.damage = 13.f;  s.speed = 125.f;  s.attackRange = 62.f;
            s.attackCooldown = 1.7f;  s.footRadius = 18.f;  s.bodySize = {62.f, 56.f};
            s.knockbackSpeed = 230.f;  s.scoreValue = 40;  s.expValue = 30;
            break;
        case SlimeType::King:  // boss
            s.maxHp = 500.f;  s.damage = 22.f;  s.speed = 120.f;  s.attackRange = 110.f;
            s.attackCooldown = 1.6f;  s.attackDuration = 0.7f;  s.attackHitTime = 0.4f;
            s.footRadius = 34.f;  s.bodySize = {110.f, 100.f};  s.detectRange = 1300.f;
            s.knockbackSpeed = 90.f;  s.stunTime = 0.1f;   // boss rất khó bị đẩy lùi
            s.scoreValue = 300;  s.expValue = 150;
            break;
    }
    // Độ khó theo level: nhân các chỉ số cơ bản với hệ số (công thức nằm trong Balance.hpp)
    s.maxHp *= scale.hp;
    s.damage *= scale.damage;
    s.speed *= scale.speed;
    s.attackCooldown *= scale.cooldown;
    return s;
}

MonsterVisual Slime::makeVisual(SlimeType type) {
    MonsterVisual v;
    if (type == SlimeType::King) {
        v.frameSize = {80, 64};
        v.origin = {40.f, 61.f};
        v.shadowScale = 5.2f;
        v.hpBarHeight = 160.f;
    } else {
        v.frameSize = {48, 40};
        v.origin = {24.f, 38.f};
        v.shadowScale = 3.2f;
        v.hpBarHeight = 95.f;
    }
    const float hopTime = (type == SlimeType::Purple) ? 0.07f : (type == SlimeType::King ? 0.11f : 0.09f);
    v.idle = {kRowIdle, 4, 0.25f, true};
    v.move = {kRowHop, 6, hopTime, true};
    v.attack = {kRowAttack, 4, (type == SlimeType::King ? 0.7f : 0.48f) / 4.f, false};
    v.hurt = {kRowHurt, 2, 0.12f, false};
    v.death = {kRowDeath, 4, 0.12f, false};
    return v;
}

float Slime::movementFactor() const {
    if (getAnimationRow() != kRowHop) return 1.f;
    const int frame = getAnimationFrame();
    return (frame >= kAirFirst && frame <= kAirLast) ? 1.f : 0.08f;  // trên không: chạy hết tốc độ; lúc ép/tiếp đất: gần như đứng yên
}

float Slime::visualLift() const {
    if (getAnimationRow() != kRowHop) return 0.f;
    const float unit = isBoss() ? 1.8f : 1.f;  // boss nảy cao hơn
    switch (getAnimationFrame()) {
        case 2: return 6.f * unit;
        case 3: return 12.f * unit;
        case 4: return 6.f * unit;
        default: return 0.f;
    }
}
