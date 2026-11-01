#include "Effects.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <cmath>

namespace {
const float kPi = 3.14159265f;

// Xóa các phần tử hết tuổi thọ (age >= life) bằng erase-remove_if.
template <typename T>
void removeExpired(std::vector<T>& items) {
    items.erase(std::remove_if(items.begin(), items.end(), [](const T& e) { return e.age >= e.life; }), items.end());
}

// Thêm phần tử; vượt giới hạn thì bỏ phần tử CŨ NHẤT (đầu vector) để vector không phình ra.
template <typename T>
void pushCapped(std::vector<T>& items, T value, std::size_t maxCount) {
    if (items.size() >= maxCount) items.erase(items.begin());
    items.push_back(std::move(value));
}
}  // namespace

void Effects::clear() {
    m_slashes.clear();
    m_sparks.clear();
    m_texts.clear();
}

void Effects::addSlash(sf::Vector2f origin, sf::Vector2f dir, float radius, float spanDeg, sf::Color color) {
    Slash s;
    s.origin = origin;
    s.angle = std::atan2(dir.y, dir.x);
    s.radius = radius;
    s.span = spanDeg * kPi / 180.f;
    s.color = color;
    pushCapped(m_slashes, s, kMaxSlashes);
}

void Effects::addSparks(sf::Vector2f position, sf::Color color, int count) {
    for (int i = 0; i < count; ++i) {
        Spark s;
        s.position = position;
        const float angle = util::randomFloat(0.f, 2.f * kPi);
        const float speed = util::randomFloat(60.f, 230.f);
        s.velocity = {std::cos(angle) * speed, std::sin(angle) * speed - 60.f};
        s.color = color;
        s.life = util::randomFloat(0.25f, 0.5f);
        s.size = util::randomFloat(2.f, 4.5f);
        pushCapped(m_sparks, s, kMaxSparks);
    }
}

void Effects::addText(sf::Vector2f position, const std::string& text, sf::Color color, unsigned size) {
    FloatingText t;
    t.position = {position.x + util::randomFloat(-10.f, 10.f), position.y};
    t.text = text;
    t.color = color;
    t.size = size;
    pushCapped(m_texts, t, kMaxTexts);
}

void Effects::update(float dt) {
    for (Slash& s : m_slashes) s.age += dt;
    for (Spark& s : m_sparks) {
        s.age += dt;
        s.position += s.velocity * dt;
        s.velocity *= std::exp(-3.f * dt);   // ma sát
        s.velocity.y += 260.f * dt;          // trọng lực nhẹ
    }
    for (FloatingText& t : m_texts) {
        t.age += dt;
        t.position.y -= 46.f * dt;           // bay lên
    }
    removeExpired(m_slashes);
    removeExpired(m_sparks);
    removeExpired(m_texts);
}

void Effects::drawWorld(sf::RenderTarget& target, const sf::Font* font) const {
    const sf::RenderStates additive{sf::BlendAdd};

    // ---- Vệt chém: dải tam giác cong; đầu vệt quét qua cung trong ~45% đầu, đuôi đuổi theo và mờ dần ----
    for (const Slash& s : m_slashes) {
        const float t = util::clampf(s.age / s.life, 0.f, 1.f);
        const float head = util::clampf(t / 0.45f, 0.f, 1.f);
        const float tail = util::clampf((t - 0.15f) / 0.85f, 0.f, 1.f);
        const int segments = 14;
        sf::VertexArray strip(sf::PrimitiveType::TriangleStrip, static_cast<std::size_t>((segments + 1) * 2));
        for (int i = 0; i <= segments; ++i) {
            const float k = static_cast<float>(i) / static_cast<float>(segments);   // 0 = đuôi, 1 = đầu
            const float u = tail + (head - tail) * k;
            const float angle = s.angle - s.span / 2.f + s.span * u;
            const float thickness = s.radius * 0.30f * std::sin(kPi * k) * (1.f - 0.5f * t);  // thon ở hai đầu
            const sf::Vector2f outer = s.origin + sf::Vector2f(std::cos(angle), std::sin(angle)) * s.radius;
            const sf::Vector2f inner = s.origin + sf::Vector2f(std::cos(angle), std::sin(angle)) * (s.radius - thickness);
            sf::Color c = s.color;
            c.a = static_cast<std::uint8_t>(255.f * (1.f - t) * (0.25f + 0.75f * k));
            strip[static_cast<std::size_t>(i * 2)].position = outer;
            strip[static_cast<std::size_t>(i * 2)].color = c;
            strip[static_cast<std::size_t>(i * 2 + 1)].position = inner;
            strip[static_cast<std::size_t>(i * 2 + 1)].color = sf::Color(c.r, c.g, c.b, static_cast<std::uint8_t>(c.a / 3));
        }
        target.draw(strip, additive);
    }

    // ---- Tia lửa khi trúng đòn ----
    for (const Spark& s : m_sparks) {
        const float t = s.age / s.life;
        sf::RectangleShape dot({s.size, s.size});
        dot.setOrigin({s.size / 2.f, s.size / 2.f});
        dot.setPosition({std::round(s.position.x), std::round(s.position.y)});
        sf::Color c = s.color;
        c.a = static_cast<std::uint8_t>(255.f * (1.f - t));
        dot.setFillColor(c);
        target.draw(dot, additive);
    }

    // ---- Số sát thương bay ----
    if (font != nullptr) {
        for (const FloatingText& t : m_texts) {
            sf::Text text(*font, util::utf8(t.text), t.size);
            const float life = t.age / t.life;
            sf::Color c = t.color;
            c.a = static_cast<std::uint8_t>(255.f * (life < 0.6f ? 1.f : (1.f - life) / 0.4f));
            text.setFillColor(c);
            text.setOutlineColor(sf::Color(10, 10, 10, c.a));
            text.setOutlineThickness(2.f);
            const sf::FloatRect b = text.getLocalBounds();
            text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y});
            text.setPosition({std::round(t.position.x), std::round(t.position.y)});
            target.draw(text);
        }
    }
}

// ============================================================================
//  Hạt môi trường (số lượng cố định, quay vòng khi ra khỏi bản đồ)
// ============================================================================
void Effects::setAmbient(AmbientType type, sf::Color color, sf::Vector2f mapSize) {
    m_ambientType = type;
    m_ambientColor = color;
    m_mapSize = mapSize;
    m_ambient.clear();
    if (type == AmbientType::None) return;
    const int count = (type == AmbientType::Dust) ? 60 : 70;
    for (int i = 0; i < count; ++i) {
        AmbientParticle p;
        p.position = {util::randomFloat(0.f, mapSize.x), util::randomFloat(0.f, mapSize.y)};
        p.phase = util::randomFloat(0.f, 2.f * kPi);
        p.speed = util::randomFloat(0.6f, 1.4f);
        p.size = (type == AmbientType::Dust) ? util::randomFloat(1.5f, 3.f) : util::randomFloat(2.f, 4.f);
        m_ambient.push_back(p);
    }
}

void Effects::updateAmbient(float dt) {
    m_ambientTime += dt;
    for (AmbientParticle& p : m_ambient) {
        switch (m_ambientType) {
            case AmbientType::Fireflies:  // bay lang thang uốn lượn
                p.position.x += std::sin(m_ambientTime * 0.7f * p.speed + p.phase) * 22.f * dt;
                p.position.y += std::cos(m_ambientTime * 0.5f * p.speed + p.phase * 1.3f) * 16.f * dt;
                break;
            case AmbientType::Embers:     // tàn lửa bay lên
                p.position.y -= (22.f + 30.f * p.speed) * dt;
                p.position.x += std::sin(m_ambientTime + p.phase) * 18.f * dt;
                break;
            case AmbientType::Dust:       // bụi trôi chậm
                p.position.x += 8.f * p.speed * dt;
                p.position.y += std::sin(m_ambientTime * 0.4f + p.phase) * 5.f * dt;
                break;
            case AmbientType::None:
                break;
        }
        if (p.position.x < 0.f) p.position.x += m_mapSize.x;      // quay vòng khi ra khỏi bản đồ
        if (p.position.x > m_mapSize.x) p.position.x -= m_mapSize.x;
        if (p.position.y < 0.f) p.position.y += m_mapSize.y;
        if (p.position.y > m_mapSize.y) p.position.y -= m_mapSize.y;
    }
}

void Effects::drawAmbient(sf::RenderTarget& target, const sf::FloatRect& view) const {
    if (m_ambientType == AmbientType::None) return;
    const sf::RenderStates additive{sf::BlendAdd};
    for (const AmbientParticle& p : m_ambient) {
        if (p.position.x < view.position.x - 10.f || p.position.x > view.position.x + view.size.x + 10.f ||
            p.position.y < view.position.y - 10.f || p.position.y > view.position.y + view.size.y + 10.f)
            continue;  // ngoài khung nhìn thì không vẽ
        float alpha = 150.f;
        if (m_ambientType == AmbientType::Fireflies) alpha = 60.f + 170.f * (0.5f + 0.5f * std::sin(m_ambientTime * 2.f * p.speed + p.phase));
        else if (m_ambientType == AmbientType::Dust) alpha = 55.f;
        sf::CircleShape dot(p.size);
        dot.setOrigin({p.size, p.size});
        dot.setPosition({std::round(p.position.x), std::round(p.position.y)});
        sf::Color c = m_ambientColor;
        c.a = static_cast<std::uint8_t>(alpha);
        dot.setFillColor(c);
        target.draw(dot, additive);
    }
}
