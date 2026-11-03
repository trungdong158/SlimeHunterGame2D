#include "Player.hpp"

#include "Assets.hpp"
#include "Effects.hpp"
#include "GameMap.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cmath>

namespace {

const float kScale = 3.f;
const int kCellW = 40;
const int kCellH = 40;
const sf::Vector2f kOrigin(20.f, 38.f);
const sf::Vector2f kHandOffset(4.3f, -8.1f);
const sf::Vector2f kBatPivot(1.5f, 2.5f);
const float kRadToDeg = 57.2957795f;

const float kFootRadius = 11.f;
const float kRunMultiplier = 1.55f;
const float kAttackDuration = 0.30f;
const float kAttackHitTime = 0.14f;
const float kInvincibleTime = 0.9f;
const float kHurtTime = 0.25f;

const AnimationClip kIdleClip{0, 4, 0.25f, true};
const AnimationClip kWalkClip{1, 6, 0.09f, true};
const AnimationClip kHurtClip{3, 2, 0.12f, false};
const AnimationClip kDeathClip{4, 4, 0.18f, false};

}  // namespace

Player::Player(const Assets& assets)
    : Entity({0.f, 0.f}, 100.f, 100.f, 20.f, 230.f, kFootRadius),
      m_sprite(assets.playerSheet),
      m_bat(assets.playerBat),
      m_shadow(assets.shadow),
      m_animation(sf::Vector2i(kCellW, kCellH)),
      m_weapon(std::make_unique<BaseballBat>()) {
    m_sprite.setOrigin(kOrigin);
    m_bat.setOrigin(kBatPivot);
    m_shadow.setOrigin({13.f, 5.f});
    m_shadow.setScale({kScale * 0.75f, kScale * 0.75f});
    reset({0.f, 0.f});
}

void Player::reset(sf::Vector2f startPosition) {
    m_position = startPosition;
    m_knockback = {0.f, 0.f};
    m_aim = {1.f, 0.f};
    m_facingRight = true;
    m_state = PlayerState::Idle;
    m_level = 1;
    m_exp = 0;
    m_coins = 0;
    m_dashTimer = 0.f;
    m_dashCooldown = 0.f;
    m_speedBoostTimer = 0.f;
    m_speedBoostMultiplier = 1.f;
    m_magnetPulseTimer = 0.f;
    m_critChance = 0.05f;
    m_attackSpeedBonus = 0.f;
    m_hasBurnAura = false;
    m_magnetRadius = 120.f;
    m_weapon = std::make_unique<BaseballBat>();

    recalculateStats();
    m_hp = m_maxHp;
    m_attackCooldown = 0.f;
    m_attackTimer = 0.f;
    m_attackHitTriggered = false;
    m_attackHitPending = false;
    m_invincibleTimer = 0.f;
    m_hurtTimer = 0.f;
    m_animation.play(kIdleClip);
    m_animation.restart();
    m_animation.setSpeed(1.f);
    updateVisuals();
}

void Player::setWeapon(std::unique_ptr<Weapon> weapon) {
    if (weapon) {
        m_weapon = std::move(weapon);
    }
}

bool Player::tryDash() {
    if (m_dashCooldown > 0.f || m_state == PlayerState::Dead) return false;
    m_dashTimer = kDashDuration;
    m_dashCooldown = kDashCooldownMax;
    m_invincibleTimer = kDashDuration + 0.15f;  // I-frames: bất tử trong khi lướt
    m_dashDir = util::length(m_aim) > 0.1f ? m_aim : sf::Vector2f(m_facingRight ? 1.f : -1.f, 0.f);
    return true;
}

float Player::getDashCooldownRatio() const {
    return m_dashCooldown <= 0.f ? 0.f : m_dashCooldown / kDashCooldownMax;
}

void Player::applySpeedBoost(float duration, float multiplier) {
    m_speedBoostTimer = std::max(m_speedBoostTimer, duration);
    m_speedBoostMultiplier = std::max(m_speedBoostMultiplier, multiplier);
}

void Player::update(float dt, const GameMap& map) {
    PlayerInput emptyInput;
    update(dt, emptyInput, map);
}

void Player::update(float dt, const PlayerInput& input, const GameMap& map) {
    static std::vector<Projectile> s_dummyProjectiles;
    static Effects s_dummyEffects;
    s_dummyProjectiles.clear();
    s_dummyEffects.clear();
    updateWithCombat(dt, input, map, s_dummyProjectiles, s_dummyEffects);
}

void Player::updateWithCombat(float dt, const PlayerInput& input, const GameMap& map,
                             std::vector<Projectile>& projectiles, Effects& effects) {
    m_animation.update(dt);

    m_attackCooldown = std::max(0.f, m_attackCooldown - dt);
    m_dashCooldown = std::max(0.f, m_dashCooldown - dt);
    m_invincibleTimer = std::max(0.f, m_invincibleTimer - dt);
    m_hurtTimer = std::max(0.f, m_hurtTimer - dt);
    m_speedBoostTimer = std::max(0.f, m_speedBoostTimer - dt);
    m_magnetPulseTimer = std::max(0.f, m_magnetPulseTimer - dt);

    if (util::length(m_knockback) > 1.f) {
        m_position = map.moveAndSlide(m_position, m_knockback * dt, kFootRadius);
        m_knockback *= std::exp(-9.f * dt);
    } else {
        m_knockback = {0.f, 0.f};
    }

    if (m_state == PlayerState::Dead) {
        updateVisuals();
        return;
    }

    // 1) Xử lý Lướt (Dash)
    if (input.dash) {
        tryDash();
    }

    if (isDashing()) {
        m_dashTimer -= dt;
        const float dashSpeed = m_speed * 3.4f;
        m_position = map.moveAndSlide(m_position, m_dashDir * (dashSpeed * dt), kFootRadius);
        // Hiệu ứng bóng mờ khi dash
        effects.addSparks(m_position + sf::Vector2f(0.f, -20.f), sf::Color(120, 210, 255, 140), 2);
    } else {
        // 2) Di chuyển bình thường
        const sf::Vector2f dir = util::normalized(input.move);
        const bool moving = util::length(dir) > 0.f;
        if (moving) {
            m_aim = dir;
            if (dir.x > 0.1f) m_facingRight = true;
            else if (dir.x < -0.1f) m_facingRight = false;
        }

        float speed = m_speed * (input.run ? kRunMultiplier : 1.f);
        if (m_speedBoostTimer > 0.f) speed *= m_speedBoostMultiplier;
        if (m_state == PlayerState::Attack) speed *= 0.35f;

        m_position = map.moveAndSlide(m_position, dir * speed * dt, kFootRadius);

        // 3) Tấn công
        if (input.attack && m_state != PlayerState::Attack && m_attackCooldown <= 0.f) {
            startAttack(projectiles, effects);
        }

        if (m_state == PlayerState::Attack) {
            m_attackTimer += dt;
            if (!m_attackHitTriggered && m_attackTimer >= kAttackHitTime) {
                m_attackHitTriggered = true;
                if (m_weapon && m_weapon->getType() == WeaponType::BaseballBat) {
                    m_attackHitPending = true;
                }
            }
            if (m_attackTimer >= kAttackDuration) m_state = PlayerState::Idle;
        }

        if (m_state != PlayerState::Attack) m_state = moving ? PlayerState::Walk : PlayerState::Idle;
        chooseAnimation(input.run);
    }

    updateVisuals();
}

void Player::startAttack(std::vector<Projectile>& projectiles, Effects& effects) {
    m_state = PlayerState::Attack;
    m_attackTimer = 0.f;
    m_attackHitTriggered = false;
    m_attackHitPending = false;

    if (m_weapon) {
        m_weapon->attack(*this, m_aim, projectiles, effects);
        m_attackCooldown = m_weapon->getCooldown() / getAttackSpeedMultiplier();
    } else {
        m_attackCooldown = 0.45f;
    }

    const AnimationClip attackClip{2, 4, kAttackDuration / 4.f, false};
    m_animation.play(attackClip);
    m_animation.restart();
    m_animation.setSpeed(1.f);
}

void Player::chooseAnimation(bool running) {
    if (m_state == PlayerState::Dead) {
        m_animation.play(kDeathClip);
        return;
    }
    if (m_state == PlayerState::Attack) return;
    if (m_hurtTimer > 0.f) {
        m_animation.play(kHurtClip);
        return;
    }
    if (m_state == PlayerState::Walk) {
        m_animation.play(kWalkClip);
        m_animation.setSpeed(running ? 1.5f : 1.f);
    } else {
        m_animation.play(kIdleClip);
        m_animation.setSpeed(1.f);
    }
}

void Player::updateVisuals() {
    m_sprite.setTextureRect(m_animation.getFrameRect());
    m_sprite.setScale({m_facingRight ? kScale : -kScale, kScale});
    const sf::Vector2f drawPos(std::round(m_position.x), std::round(m_position.y));
    m_sprite.setPosition(drawPos);
    m_shadow.setPosition({drawPos.x, drawPos.y - 3.f});

    sf::Color color = sf::Color::White;
    if (m_hurtTimer > 0.f) color = sf::Color(255, 110, 110);
    if (isDashing()) color = sf::Color(140, 220, 255, 200);  // Nhuốm xanh phát sáng khi lướt
    else if (m_invincibleTimer > 0.f && m_state != PlayerState::Dead) {
        const bool dim = static_cast<int>(m_invincibleTimer * 14.f) % 2 == 0;
        color.a = dim ? 120 : 255;
    }
    m_sprite.setColor(color);

    const float side = m_facingRight ? 1.f : -1.f;
    const float restDeg = m_facingRight ? -25.f : -155.f;
    float angleDeg = restDeg;
    if (m_state == PlayerState::Attack) {
        const float t = util::clampf(m_attackTimer / kAttackDuration, 0.f, 1.f);
        const float aimDeg = std::atan2(m_aim.y, m_aim.x) * kRadToDeg;
        const float windUp = aimDeg - 100.f;
        const float follow = aimDeg + 70.f;
        if (t < 0.3f) angleDeg = util::lerpAngleDeg(restDeg, windUp, util::easeOutQuad(t / 0.3f));
        else if (t < 0.6f) angleDeg = util::lerpAngleDeg(windUp, follow, util::easeOutQuad((t - 0.3f) / 0.3f));
        else angleDeg = util::lerpAngleDeg(follow, restDeg, (t - 0.6f) / 0.4f);
    }
    m_bat.setPosition({drawPos.x + kHandOffset.x * kScale * side, drawPos.y + kHandOffset.y * kScale});
    m_bat.setScale({kScale, m_facingRight ? kScale : -kScale});
    m_bat.setRotation(sf::degrees(angleDeg));
    m_bat.setColor(color);
}

void Player::drawShadow(sf::RenderTarget& target) const { target.draw(m_shadow); }

void Player::draw(sf::RenderTarget& target) const {
    target.draw(m_sprite);
    // Chỉ vẽ gậy khi đang trang bị BaseballBat
    if (m_state != PlayerState::Dead && m_weapon && m_weapon->getType() == WeaponType::BaseballBat) {
        target.draw(m_bat);
    }
}

sf::FloatRect Player::getHitbox() const {
    return sf::FloatRect({m_position.x - 16.f, m_position.y - 48.f}, {32.f, 48.f});
}

sf::FloatRect Player::getAttackHitbox() const {
    if (m_weapon) {
        return m_weapon->getHitbox(*this);
    }
    const float size = m_slash.size;
    const sf::Vector2f center = m_position + sf::Vector2f(0.f, -40.f) + m_aim * m_slash.reach;
    return sf::FloatRect({center.x - size / 2.f, center.y - size / 2.f}, {size, size});
}

bool Player::takeDamage(float amount, sf::Vector2f fromPosition) {
    if (m_state == PlayerState::Dead || m_invincibleTimer > 0.f || isDashing()) return false;

    m_hp -= amount;
    m_invincibleTimer = kInvincibleTime;
    m_hurtTimer = kHurtTime;
    m_knockback = util::normalized(m_position - fromPosition) * 260.f;

    if (m_hp <= 0.f) {
        m_hp = 0.f;
        m_state = PlayerState::Dead;
        m_attackHitPending = false;
        m_animation.play(kDeathClip);
        m_animation.restart();
        m_animation.setSpeed(1.f);
    }
    return true;
}

int Player::getExpToNext() const { return 40 + (m_level - 1) * 25; }

bool Player::addExp(int amount) {
    m_exp += amount;
    bool leveledUp = false;
    while (m_exp >= getExpToNext()) {
        m_exp -= getExpToNext();
        ++m_level;
        recalculateStats();
        m_hp = std::min(m_maxHp, m_hp + 30.f);
        leveledUp = true;
    }
    return leveledUp;
}

void Player::recalculateStats() {
    const float levelsGained = static_cast<float>(m_level - 1);
    m_maxHp = static_cast<float>(m_baseMaxHp) + levelsGained * balance::kExpLevelHpBonus;
    m_damage = static_cast<float>(m_baseDamage) + levelsGained * balance::kExpLevelDamageBonus;
    m_hp = std::min(m_hp, m_maxHp);
}

void Player::setBaseStats(int maxHp, int damage) {
    m_baseMaxHp = std::max(maxHp, 1);
    m_baseDamage = std::max(damage, 1);
    recalculateStats();
}

void Player::applyLevelUpgrade(int newMaxHp, int newDamage) {
    setBaseStats(newMaxHp, newDamage);
    m_hp = m_maxHp;
}

void Player::heal(float amount) {
    if (m_state == PlayerState::Dead || amount <= 0.f) return;
    m_hp = std::min(m_maxHp, m_hp + amount);
}

void Player::setSlashTier(int tier) {
    m_slashTier = std::min(std::max(tier, 1), balance::kMaxSlashTier);
    m_slash = balance::slashProfileForTier(m_slashTier);
}

void Player::teleportTo(sf::Vector2f position) {
    m_position = position;
    m_knockback = {0.f, 0.f};
    if (m_state != PlayerState::Dead) m_state = PlayerState::Idle;
    m_attackCooldown = 0.f;
    m_attackTimer = 0.f;
    m_attackHitTriggered = false;
    m_attackHitPending = false;
    m_hurtTimer = 0.f;
    m_invincibleTimer = 1.0f;
    m_dashTimer = 0.f;
    m_dashCooldown = 0.f;
    m_animation.play(kIdleClip);
    m_animation.restart();
    m_animation.setSpeed(1.f);
    updateVisuals();
}
