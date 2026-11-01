#include "Monster.hpp"

#include "GameMap.hpp"
#include "PathField.hpp"
#include "Player.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
const float kFadeTime = 0.5f;
}

Monster::Monster(const sf::Texture& sheet, const sf::Texture& shadow, const MonsterStats& stats,
                 const MonsterVisual& visual, sf::Vector2f startPosition)
    : Entity(startPosition, stats.maxHp, stats.maxHp, stats.damage, stats.speed, stats.footRadius),
      m_sprite(sheet),
      m_shadow(shadow),
      m_animation(visual.frameSize),
      m_stats(stats),
      m_visual(visual) {
    m_sprite.setOrigin(m_visual.origin);
    m_shadow.setOrigin({13.f, 5.f});
    m_shadow.setScale({m_visual.shadowScale, m_visual.shadowScale});
    m_wanderTimer = util::randomFloat(0.2f, 2.f);
    m_animation.play(m_visual.idle);
    updateVisuals();
}

void Monster::update(float dt, const GameMap& map) {
    (void)dt;
    (void)map;
}

void Monster::update(float dt, const Player& player, const GameMap& map, const PathField& pathToPlayer) {
    static std::vector<Projectile> s_dummyProjectiles;
    s_dummyProjectiles.clear();
    updateWithPlayer(dt, player, map, pathToPlayer, s_dummyProjectiles);
}

void Monster::performRangedAttack(std::vector<Projectile>& projectiles, const sf::Vector2f& playerPos) {
    (void)projectiles;
    (void)playerPos;
}

void Monster::updateWithPlayer(float dt, const Player& player, const GameMap& map,
                              const PathField& pathToPlayer, std::vector<Projectile>& projectiles) {
    m_animation.setSpeed(m_state == MonsterState::Dead ? 1.f : getSlowFactor());
    m_animation.update(dt);
    m_flashTimer = std::max(0.f, m_flashTimer - dt);

    if (m_state == MonsterState::Dead) {
        m_deathTimer += dt;
        applyKnockback(dt, map);
        updateVisuals();
        return;
    }

    m_attackCooldown = std::max(0.f, m_attackCooldown - dt);
    m_stunTimer = std::max(0.f, m_stunTimer - dt);
    updateStatusEffects(dt);
    if (m_state == MonsterState::Dead) {
        updateVisuals();
        return;
    }
    applyKnockback(dt, map);

    if (m_stunTimer > 0.f) {
        if (m_state == MonsterState::Attack) {
            m_state = MonsterState::Chase;
            m_attackHitPending = false;
        }
        m_animation.play(m_visual.hurt);
        updateVisuals();
        return;
    }

    const sf::Vector2f toPlayer = player.getPosition() - m_position;
    const float dist = util::length(toPlayer);
    const bool playerAlive = !player.isDead();

    switch (m_state) {
        case MonsterState::Idle:
        case MonsterState::Chase:
            if (playerAlive && dist <= m_stats.attackRange && m_attackCooldown <= 0.f) {
                startAttack();
                if (isRanged()) {
                    performRangedAttack(projectiles, player.getPosition());
                }
            } else if (playerAlive && dist <= m_stats.detectRange) {
                m_state = MonsterState::Chase;
                if (isRanged()) {
                    // Quái tầm xa (Skeleton): nếu người chơi áp sát (< 80px) thì lùi nhẹ
                    if (dist < 80.f) {
                        m_animation.play(m_visual.move);
                        const sf::Vector2f awayDir = util::normalized(m_position - player.getPosition());
                        moveToward(awayDir, 0.35f, dt, map);
                    } else if (dist <= m_stats.attackRange * 0.90f) {
                        m_animation.play(m_visual.idle);
                    } else {
                        m_animation.play(m_visual.move);
                        moveToward(chooseChaseDirection(player, map, pathToPlayer), 1.f, dt, map);
                    }
                } else {
                    // Quái cận chiến
                    if (dist > m_stats.attackRange * 0.85f) {
                        m_animation.play(m_visual.move);
                        moveToward(chooseChaseDirection(player, map, pathToPlayer), 1.f, dt, map);
                    } else {
                        m_animation.play(m_visual.idle);
                    }
                }
            } else {
                m_state = MonsterState::Idle;
                wander(dt, map);
            }
            break;

        case MonsterState::Attack:
            m_attackTimer += dt;
            if (!m_attackHitTriggered && m_attackTimer >= m_stats.attackHitTime) {
                m_attackHitTriggered = true;
                if (!isRanged()) {
                    m_attackHitPending = true;
                }
            }
            if (m_attackTimer >= m_stats.attackDuration) {
                m_state = MonsterState::Chase;
                m_attackCooldown = m_stats.attackCooldown;
            }
            break;

        case MonsterState::Dead:
            break;
    }
    updateVisuals();
}

void Monster::startAttack() {
    m_state = MonsterState::Attack;
    m_attackTimer = 0.f;
    m_attackHitTriggered = false;
    m_attackHitPending = false;
    m_animation.play(m_visual.attack);
    m_animation.restart();
}

sf::Vector2f Monster::chooseChaseDirection(const Player& player, const GameMap& map, const PathField& pathToPlayer) const {
    const sf::Vector2f target = player.getPosition();
    if (ignoresObstacles() || map.isLineFree(m_position, target, m_stats.footRadius)) {
        return util::normalized(target - m_position);
    }

    const sf::Vector2f viaPath = pathToPlayer.getDirection(map, m_position, m_stats.footRadius);
    if (util::length(viaPath) > 0.01f) return viaPath;
    return util::normalized(target - m_position);
}

void Monster::moveToward(sf::Vector2f direction, float speedScale, float dt, const GameMap& map) {
    if (m_sidestepTimer > 0.f) {
        m_sidestepTimer -= dt;
        direction = m_sidestepDir;
    }
    if (std::fabs(direction.x) > 0.2f) m_facingRight = direction.x > 0.f;
    const float speed = m_stats.speed * movementFactor() * speedScale * getSlowFactor();

    const sf::Vector2f before = m_position;
    if (ignoresObstacles()) {
        m_position += direction * (speed * dt);
    } else {
        m_position = map.moveAndSlide(m_position, direction * speed * dt, m_stats.footRadius);
    }

    if (!ignoresObstacles() && speed > 1.f && util::distance(before, m_position) < speed * dt * 0.25f) {
        m_stuckTimer += dt;
        if (m_stuckTimer > 0.4f) {
            m_stuckTimer = 0.f;
            m_sidestepTimer = 0.4f;
            const float angle = util::randomFloat(1.0f, 2.1f) * (util::randomInt(0, 1) == 0 ? 1.f : -1.f);
            const sf::Vector2f d = util::normalized(direction);
            m_sidestepDir = {d.x * std::cos(angle) - d.y * std::sin(angle), d.x * std::sin(angle) + d.y * std::cos(angle)};
        }
    } else {
        m_stuckTimer = 0.f;
    }
}

void Monster::wander(float dt, const GameMap& map) {
    m_wanderTimer -= dt;
    if (m_wanderTimer <= 0.f) {
        m_wanderTimer = util::randomFloat(1.2f, 3.f);
        if (util::randomInt(0, 1) == 0) {
            m_wanderDir = {0.f, 0.f};
        } else {
            const float angle = util::randomFloat(0.f, 6.2831853f);
            m_wanderDir = {std::cos(angle), std::sin(angle)};
        }
    }
    if (util::length(m_wanderDir) > 0.f) {
        m_animation.play(m_visual.move);
        moveToward(m_wanderDir, 0.3f, dt, map);
    } else {
        m_animation.play(m_visual.idle);
    }
}

void Monster::applyKnockback(float dt, const GameMap& map) {
    if (util::length(m_knockback) > 4.f) {
        if (ignoresObstacles()) {
            m_position += m_knockback * dt;
        } else {
            m_position = map.moveAndSlide(m_position, m_knockback * dt, m_stats.footRadius);
        }
        m_knockback *= std::exp(-8.f * dt);
    } else {
        m_knockback = {0.f, 0.f};
    }
}

bool Monster::takeDamage(float amount, sf::Vector2f fromPosition) {
    if (m_state == MonsterState::Dead) return false;

    m_hp -= amount;
    m_flashTimer = 0.15f;
    m_stunTimer = m_stats.stunTime;
    m_knockback = util::normalized(m_position - fromPosition) * m_stats.knockbackSpeed;

    if (m_hp <= 0.f) {
        die();
        return true;
    }
    return false;
}

void Monster::die() {
    m_hp = 0.f;
    m_deathRewardPending = true;
    m_state = MonsterState::Dead;
    m_attackHitPending = false;
    m_deathTimer = 0.f;
    m_animation.play(m_visual.death);
    m_animation.restart();
}

bool Monster::consumeAttackHit() {
    if (m_state == MonsterState::Dead || !m_attackHitPending) return false;
    m_attackHitPending = false;
    return true;
}

void Monster::nudge(sf::Vector2f delta, const GameMap& map) {
    if (ignoresObstacles()) {
        m_position += delta;
    } else {
        m_position = map.moveAndSlide(m_position, delta, m_stats.footRadius);
    }
}

bool Monster::shouldRemove() const {
    if (m_state != MonsterState::Dead) return false;
    const float deathAnimTime = static_cast<float>(m_visual.death.frameCount) * m_visual.death.frameTime;
    return m_deathTimer >= deathAnimTime + kFadeTime;
}

sf::FloatRect Monster::getHitbox() const {
    return sf::FloatRect({m_position.x - m_stats.bodySize.x / 2.f, m_position.y - m_stats.bodySize.y}, m_stats.bodySize);
}

void Monster::updateVisuals() {
    m_sprite.setTextureRect(m_animation.getFrameRect());
    m_sprite.setScale({m_facingRight ? m_visual.scale : -m_visual.scale, m_visual.scale});

    const float lift = (m_state == MonsterState::Dead) ? 0.f : visualLift();
    m_sprite.setPosition({std::round(m_position.x), std::round(m_position.y - lift)});
    m_shadow.setPosition({std::round(m_position.x), std::round(m_position.y) - 3.f});

    sf::Color color = sf::Color::White;
    if (m_slowTimer > 0.f) color = sf::Color(140, 190, 255);
    if (m_dotTimer > 0.f && static_cast<int>(m_dotTimer * 6.f) % 2 == 0) color = sf::Color(255, 170, 80);
    if (m_flashTimer > 0.f) color = sf::Color(255, 120, 120);
    if (m_state == MonsterState::Dead) {
        const float deathAnimTime = static_cast<float>(m_visual.death.frameCount) * m_visual.death.frameTime;
        const float fade = util::clampf((m_deathTimer - deathAnimTime) / kFadeTime, 0.f, 1.f);
        color.a = static_cast<std::uint8_t>(255.f * (1.f - fade));
    }
    m_sprite.setColor(color);
}

void Monster::drawShadow(sf::RenderTarget& target) const {
    if (m_state == MonsterState::Dead && m_deathTimer > 0.4f) return;
    target.draw(m_shadow);
}

void Monster::draw(sf::RenderTarget& target) const {
    target.draw(m_sprite);

    if (m_state != MonsterState::Dead && m_hp < m_stats.maxHp) {
        const float width = m_stats.bodySize.x;
        const float ratio = util::clampf(m_hp / m_stats.maxHp, 0.f, 1.f);
        const float x = std::round(m_position.x - width / 2.f);
        const float y = std::round(m_position.y - m_visual.hpBarHeight);

        sf::RectangleShape back({width + 4.f, 9.f});
        back.setPosition({x - 2.f, y - 2.f});
        back.setFillColor(sf::Color(20, 20, 20, 210));
        target.draw(back);

        sf::RectangleShape fill({width * ratio, 5.f});
        fill.setPosition({x, y});
        fill.setFillColor(ratio > 0.5f ? sf::Color(90, 220, 90) : (ratio > 0.25f ? sf::Color(240, 200, 60) : sf::Color(235, 70, 60)));
        target.draw(fill);
    }
}

void Monster::applySlow(float duration, float speedMultiplier) {
    if (m_state == MonsterState::Dead || duration <= 0.f) return;
    const float multiplier = util::clampf(speedMultiplier, 0.2f, 1.f);
    if (m_slowTimer > 0.f) m_slowMultiplier = std::min(m_slowMultiplier, multiplier);
    else m_slowMultiplier = multiplier;
    m_slowTimer = std::max(m_slowTimer, duration);
}

void Monster::applyDot(float duration, float tickInterval, float damagePerTick) {
    if (m_state == MonsterState::Dead || duration <= 0.f || damagePerTick <= 0.f) return;
    if (m_dotTimer <= 0.f) {
        m_dotTickInterval = std::max(tickInterval, 0.1f);
        m_dotTickTimer = m_dotTickInterval;
        m_dotDamage = damagePerTick;
    } else {
        m_dotDamage = std::max(m_dotDamage, damagePerTick);
    }
    m_dotTimer = std::max(m_dotTimer, duration);
}

void Monster::updateStatusEffects(float dt) {
    if (m_slowTimer > 0.f) {
        m_slowTimer = std::max(0.f, m_slowTimer - dt);
        if (m_slowTimer <= 0.f) m_slowMultiplier = 1.f;
    }
    if (m_dotTimer > 0.f) {
        m_dotTimer -= dt;
        m_dotTickTimer -= dt;
        while (m_dotTickTimer <= 0.f && m_state != MonsterState::Dead) {
            m_dotTickTimer += m_dotTickInterval;
            m_hp -= m_dotDamage;
            m_flashTimer = 0.12f;
            m_dotDamageAccum += static_cast<int>(std::lround(m_dotDamage));
            if (m_hp <= 0.f) die();
        }
        if (m_dotTimer <= 0.f) m_dotTimer = 0.f;
    }
}

int Monster::consumeDotDamage() {
    const int total = m_dotDamageAccum;
    m_dotDamageAccum = 0;
    return total;
}

bool Monster::consumeDeathReward() {
    if (!m_deathRewardPending) return false;
    m_deathRewardPending = false;
    return true;
}
