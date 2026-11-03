#include "Projectile.hpp"
#include "GameMap.hpp"
#include "Utils.hpp"

#include <cmath>

Projectile::Projectile(sf::Vector2f startPos, sf::Vector2f velocity, float damage, float lifetime,
                       float radius, bool fromPlayer, ProjectileType type, sf::Color color)
    : m_position(startPos),
      m_velocity(velocity),
      m_damage(damage),
      m_lifetime(lifetime),
      m_maxLifetime(lifetime),
      m_radius(radius),
      m_fromPlayer(fromPlayer),
      m_type(type),
      m_color(color) {
    if (util::length(velocity) > 0.001f) {
        m_rotation = std::atan2(velocity.y, velocity.x) * 57.2957795f;
    }
}

void Projectile::update(float dt, const sf::Vector2f& playerPos, const GameMap& map) {
    if (m_lifetime <= 0.f) return;
    m_lifetime -= dt;

    if (m_type == ProjectileType::Boomerang) {
        m_rotation += 720.f * dt;
        // Sau nửa thời gian sống, boomerang quay ngược lại hướng về Player
        if (m_lifetime <= m_maxLifetime * 0.5f) {
            m_isReturning = true;
            const sf::Vector2f toPlayer = playerPos - m_position;
            const sf::Vector2f dir = util::normalized(toPlayer);
            const float speed = util::length(m_velocity);
            m_velocity = dir * speed;
            // Nếu đã chạm rất gần player khi quay về thì tự biến mất
            if (util::distance(m_position, playerPos) < 25.f) {
                m_lifetime = 0.f;
                return;
            }
        }
    } else {
        // Kiểm tra va chạm với tường bản đồ (mũi tên hoặc đạn trúng tường thì vỡ)
        if (!map.isCircleFree(m_position + m_velocity * dt, m_radius)) {
            m_lifetime = 0.f;
            return;
        }
    }

    m_position += m_velocity * dt;
}

void Projectile::draw(sf::RenderTarget& target) const {
    if (m_lifetime <= 0.f) return;

    if (m_type == ProjectileType::Arrow) {
        // Vẽ mũi tên: thân chữ nhật mảnh + đầu tam giác
        sf::RectangleShape shaft({m_radius * 3.2f, 3.f});
        shaft.setOrigin({m_radius * 1.6f, 1.5f});
        shaft.setPosition(m_position);
        shaft.setRotation(sf::degrees(m_rotation));
        shaft.setFillColor(m_color);
        target.draw(shaft);

        sf::CircleShape head(4.f, 3);
        head.setOrigin({4.f, 4.f});
        head.setPosition(m_position + util::normalized(m_velocity) * (m_radius * 1.6f));
        head.setRotation(sf::degrees(m_rotation + 90.f));
        head.setFillColor(sf::Color(240, 240, 240));
        target.draw(head);
    } else if (m_type == ProjectileType::Boomerang) {
        // Vẽ boomerang: hình chữ V xoay tròn
        sf::RectangleShape wing1({m_radius * 2.2f, 4.f});
        wing1.setOrigin({0.f, 2.f});
        wing1.setPosition(m_position);
        wing1.setRotation(sf::degrees(m_rotation));
        wing1.setFillColor(m_color);
        target.draw(wing1);

        sf::RectangleShape wing2({m_radius * 2.2f, 4.f});
        wing2.setOrigin({0.f, 2.f});
        wing2.setPosition(m_position);
        wing2.setRotation(sf::degrees(m_rotation + 80.f));
        wing2.setFillColor(m_color);
        target.draw(wing2);
    } else {
        // Cầu phép (MagicOrb) hoặc Cầu độc (PoisonBall)
        sf::CircleShape glow(m_radius * 1.4f);
        glow.setOrigin({m_radius * 1.4f, m_radius * 1.4f});
        glow.setPosition(m_position);
        glow.setFillColor(sf::Color(m_color.r, m_color.g, m_color.b, 80));
        target.draw(glow);

        sf::CircleShape core(m_radius);
        core.setOrigin({m_radius, m_radius});
        core.setPosition(m_position);
        core.setFillColor(m_color);
        target.draw(core);
    }
}

bool Projectile::checkHit(const sf::FloatRect& targetBox) const {
    if (m_lifetime <= 0.f) return false;
    // Kiểm tra xem hình chữ nhật bao quanh viên đạn có giao với targetBox không
    const sf::FloatRect myBox({m_position.x - m_radius, m_position.y - m_radius},
                              {m_radius * 2.f, m_radius * 2.f});
    return myBox.findIntersection(targetBox).has_value();
}
