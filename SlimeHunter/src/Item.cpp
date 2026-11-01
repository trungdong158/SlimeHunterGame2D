#include "Item.hpp"
#include "Player.hpp"
#include "Utils.hpp"

#include <cmath>

Item::Item(sf::Vector2f pos, ItemType type)
    : m_position(pos), m_type(type), m_bobTimer(util::randomFloat(0.f, 6.28f)) {}

void Item::pullTowards(sf::Vector2f targetPos) {
    m_beingPulled = true;
    const sf::Vector2f dir = util::normalized(targetPos - m_position);
    m_position += dir * 450.f * 0.016f;
}

void Item::update(float dt, const sf::Vector2f& playerPos, float magnetRadius) {
    m_bobTimer += dt * 5.f;

    const float dist = util::distance(m_position, playerPos);
    if (dist < magnetRadius || m_beingPulled) {
        m_beingPulled = true;
        m_pullSpeed = std::min(m_pullSpeed + 800.f * dt, 550.f);
        const sf::Vector2f dir = util::normalized(playerPos - m_position);
        m_position += dir * (m_pullSpeed * dt);
    }
}

bool Item::checkCollect(const sf::Vector2f& playerPos, float playerRadius) {
    if (m_collected) return false;
    const float pickupRadius = playerRadius + 14.f;
    if (util::distance(m_position, playerPos) <= pickupRadius) {
        m_collected = true;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// HealthPotion
HealthPotion::HealthPotion(sf::Vector2f pos, float healAmount)
    : Item(pos, ItemType::HealthPotion), m_healAmount(healAmount) {}

void HealthPotion::apply(Player& player) {
    player.heal(m_healAmount);
}

void HealthPotion::draw(sf::RenderTarget& target) const {
    const float yOffset = std::sin(m_bobTimer) * 3.f;
    const sf::Vector2f pos = {m_position.x, m_position.y + yOffset};

    // Bình thuốc màu đỏ
    sf::RectangleShape bottle({14.f, 16.f});
    bottle.setOrigin({7.f, 8.f});
    bottle.setPosition(pos);
    bottle.setFillColor(sf::Color(230, 45, 55));
    bottle.setOutlineThickness(1.5f);
    bottle.setOutlineColor(sf::Color(255, 230, 230));
    target.draw(bottle);

    // Cổ bình
    sf::RectangleShape neck({6.f, 4.f});
    neck.setOrigin({3.f, 12.f});
    neck.setPosition(pos);
    neck.setFillColor(sf::Color(200, 200, 220));
    target.draw(neck);

    // Chữ thập trắng
    sf::RectangleShape c1({6.f, 2.f});
    c1.setOrigin({3.f, 1.f});
    c1.setPosition(pos);
    c1.setFillColor(sf::Color::White);
    target.draw(c1);

    sf::RectangleShape c2({2.f, 6.f});
    c2.setOrigin({1.f, 3.f});
    c2.setPosition(pos);
    c2.setFillColor(sf::Color::White);
    target.draw(c2);
}

// ----------------------------------------------------------------------------
// SpeedBooster
SpeedBooster::SpeedBooster(sf::Vector2f pos, float duration, float speedBoost)
    : Item(pos, ItemType::SpeedBooster), m_duration(duration), m_speedBoost(speedBoost) {}

void SpeedBooster::apply(Player& player) {
    player.applySpeedBoost(m_duration, m_speedBoost);
}

void SpeedBooster::draw(sf::RenderTarget& target) const {
    const float yOffset = std::sin(m_bobTimer) * 3.f;
    const sf::Vector2f pos = {m_position.x, m_position.y + yOffset};

    // Biểu tượng tia chớp / cánh giày màu xanh lơ
    sf::CircleShape glow(10.f);
    glow.setOrigin({10.f, 10.f});
    glow.setPosition(pos);
    glow.setFillColor(sf::Color(40, 180, 255, 120));
    target.draw(glow);

    sf::RectangleShape bolt({4.f, 14.f});
    bolt.setOrigin({2.f, 7.f});
    bolt.setPosition(pos);
    bolt.setRotation(sf::degrees(25.f));
    bolt.setFillColor(sf::Color(240, 255, 100));
    target.draw(bolt);
}

// ----------------------------------------------------------------------------
// MagnetItem
MagnetItem::MagnetItem(sf::Vector2f pos) : Item(pos, ItemType::Magnet) {}

void MagnetItem::apply(Player& player) {
    player.triggerMagnetPulse();
}

void MagnetItem::draw(sf::RenderTarget& target) const {
    const float yOffset = std::sin(m_bobTimer) * 3.f;
    const sf::Vector2f pos = {m_position.x, m_position.y + yOffset};

    // Hình móng ngựa nam châm (đỏ - bạc)
    sf::CircleShape horseshoe(9.f);
    horseshoe.setOrigin({9.f, 9.f});
    horseshoe.setPosition(pos);
    horseshoe.setFillColor(sf::Color(220, 40, 40));
    target.draw(horseshoe);

    sf::CircleShape inner(5.f);
    inner.setOrigin({5.f, 5.f});
    inner.setPosition({pos.x, pos.y + 2.f});
    inner.setFillColor(sf::Color(30, 30, 40));
    target.draw(inner);

    // Cực bạc
    sf::RectangleShape tip1({4.f, 3.f});
    tip1.setPosition({pos.x - 8.f, pos.y + 4.f});
    tip1.setFillColor(sf::Color(220, 220, 240));
    target.draw(tip1);

    sf::RectangleShape tip2({4.f, 3.f});
    tip2.setPosition({pos.x + 4.f, pos.y + 4.f});
    tip2.setFillColor(sf::Color(220, 220, 240));
    target.draw(tip2);
}

// ----------------------------------------------------------------------------
// BombPickup
BombPickup::BombPickup(sf::Vector2f pos, float damage)
    : Item(pos, ItemType::Bomb), m_damage(damage) {}

void BombPickup::apply(Player& player) {
    (void)player;
    // Xử lý nổ quái trong Game loop
}

void BombPickup::draw(sf::RenderTarget& target) const {
    const float yOffset = std::sin(m_bobTimer) * 3.f;
    const sf::Vector2f pos = {m_position.x, m_position.y + yOffset};

    sf::CircleShape bomb(8.f);
    bomb.setOrigin({8.f, 8.f});
    bomb.setPosition(pos);
    bomb.setFillColor(sf::Color(35, 35, 45));
    bomb.setOutlineThickness(1.5f);
    bomb.setOutlineColor(sf::Color(200, 60, 60));
    target.draw(bomb);

    // Ngòi nổ đỏ nhấp nháy
    sf::CircleShape spark(2.5f);
    spark.setOrigin({2.5f, 2.5f});
    spark.setPosition({pos.x + 4.f, pos.y - 8.f});
    spark.setFillColor(sf::Color(255, 120, 30));
    target.draw(spark);
}

// ----------------------------------------------------------------------------
// ExpGem
ExpGem::ExpGem(sf::Vector2f pos, int expAmount)
    : Item(pos, ItemType::ExpGem), m_expAmount(expAmount) {}

void ExpGem::apply(Player& player) {
    player.addExp(m_expAmount);
}

void ExpGem::draw(sf::RenderTarget& target) const {
    const float yOffset = std::sin(m_bobTimer) * 3.f;
    const sf::Vector2f pos = {m_position.x, m_position.y + yOffset};

    // Viên đá quý hình thoi màu ngọc lục bảo
    sf::CircleShape gem(6.f, 4);
    gem.setOrigin({6.f, 6.f});
    gem.setPosition(pos);
    gem.setRotation(sf::degrees(45.f));
    gem.setFillColor(sf::Color(80, 240, 160));
    gem.setOutlineThickness(1.f);
    gem.setOutlineColor(sf::Color(220, 255, 230));
    target.draw(gem);
}

// ----------------------------------------------------------------------------
// CoinItem
CoinItem::CoinItem(sf::Vector2f pos, int value)
    : Item(pos, ItemType::Coin), m_value(value) {}

void CoinItem::apply(Player& player) {
    player.addCoins(m_value);
}

void CoinItem::draw(sf::RenderTarget& target) const {
    const float yOffset = std::sin(m_bobTimer) * 3.f;
    const sf::Vector2f pos = {m_position.x, m_position.y + yOffset};

    sf::CircleShape coin(6.f);
    coin.setOrigin({6.f, 6.f});
    coin.setPosition(pos);
    coin.setFillColor(sf::Color(255, 215, 40));
    coin.setOutlineThickness(1.f);
    coin.setOutlineColor(sf::Color(200, 150, 20));
    target.draw(coin);
}

// ----------------------------------------------------------------------------
// ItemFactory
std::unique_ptr<Item> ItemFactory::createRandomDrop(sf::Vector2f pos, float dropChance) {
    const float r = util::randomFloat(0.f, 1.f);
    if (r > dropChance) return nullptr;

    const float typeRoll = util::randomFloat(0.f, 1.f);
    if (typeRoll < 0.38f) {
        return std::make_unique<ExpGem>(pos, 15);
    } else if (typeRoll < 0.62f) {
        return std::make_unique<CoinItem>(pos, 10);
    } else if (typeRoll < 0.78f) {
        return std::make_unique<HealthPotion>(pos, 35.f);
    } else if (typeRoll < 0.88f) {
        return std::make_unique<SpeedBooster>(pos, 6.f, 1.35f);
    } else if (typeRoll < 0.94f) {
        return std::make_unique<MagnetItem>(pos);
    } else {
        return std::make_unique<BombPickup>(pos, 130.f);
    }
}
