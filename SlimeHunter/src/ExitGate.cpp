#include "ExitGate.hpp"

#include "Utils.hpp"

#include <cmath>

namespace {
const float kTwoPi = 6.2831853f;
const float kGroundSquash = 0.55f;  // góc nhìn từ trên xuống: vòng tròn trên mặt đất bị "dẹt" theo chiều dọc
}  // namespace

void ExitGate::setup(sf::Vector2f position, float radius) {
    m_position = position;
    m_radius = radius;
    m_open = false;
    m_time = 0.f;
    m_openAnim = 1.f;
}

void ExitGate::setOpen(bool open) {
    if (open && !m_open) m_openAnim = 0.f;  // vừa mở: chạy hiệu ứng vòng sáng lan rộng
    m_open = open;
}

void ExitGate::update(float dt) {
    m_time += dt;
    if (m_openAnim < 1.f) m_openAnim = std::min(1.f, m_openAnim + dt / 0.8f);
}

bool ExitGate::touches(sf::Vector2f footPosition, float footRadius) const {
    return util::distance(footPosition, m_position) <= m_radius + footRadius * 0.5f;
}

void ExitGate::draw(sf::RenderTarget& target) const {
    auto ellipse = [&](float radius, sf::Color fill, sf::Color outline, float thickness, sf::BlendMode blend) {
        sf::CircleShape shape(radius, 48);
        shape.setOrigin({radius, radius});
        shape.setPosition(m_position);
        shape.setScale({1.f, kGroundSquash});
        shape.setFillColor(fill);
        shape.setOutlineColor(outline);
        shape.setOutlineThickness(thickness);
        target.draw(shape, sf::RenderStates(blend));
    };

    if (!m_open) {
        // ---- ĐÓNG: tối, màu đỏ, có dấu X ----
        const float pulse = 0.5f + 0.5f * std::sin(m_time * 2.f);
        ellipse(m_radius, sf::Color(50, 15, 25, 150), sf::Color(190, 70, 70, static_cast<std::uint8_t>(150 + 60 * pulse)), 4.f, sf::BlendAlpha);
        const float arm = m_radius * 0.38f;
        for (int sign : {-1, 1}) {
            sf::RectangleShape bar({arm * 2.4f, 5.f});
            bar.setOrigin({arm * 1.2f, 2.5f});
            bar.setPosition({m_position.x, m_position.y - 4.f});
            bar.setRotation(sf::degrees(45.f * static_cast<float>(sign)));
            bar.setFillColor(sf::Color(200, 80, 80, 200));
            target.draw(bar);
        }
        return;
    }

    // ---- MỞ: phát sáng (cộng màu - additive) ----
    const float pulse = 0.5f + 0.5f * std::sin(m_time * 3.f);
    const sf::BlendMode add = sf::BlendAdd;
    const float grow = util::easeOutQuad(m_openAnim);

    ellipse(m_radius * 1.45f, sf::Color(40, 160, 220, static_cast<std::uint8_t>(55 + 35 * pulse)), sf::Color::Transparent, 0.f, add);   // quầng sáng
    ellipse(m_radius * (0.6f + 0.4f * grow), sf::Color(120, 255, 255, static_cast<std::uint8_t>(90 + 50 * pulse)), sf::Color(170, 255, 255, 230), 5.f, add);

    // Vòng sáng lan rộng ngay lúc mở cổng
    if (m_openAnim < 1.f) {
        const float r = m_radius * (1.f + 1.6f * m_openAnim);
        ellipse(r, sf::Color::Transparent, sf::Color(200, 255, 255, static_cast<std::uint8_t>(230.f * (1.f - m_openAnim))), 6.f, add);
    }

    // Cột sáng: dải dọc mờ dần lên trên
    const float beamWidth = m_radius * 0.85f;
    const float beamHeight = 150.f * (0.4f + 0.6f * grow);
    sf::VertexArray beam(sf::PrimitiveType::TriangleStrip, 4);
    const sf::Color base(120, 240, 255, static_cast<std::uint8_t>(110 + 40 * pulse));
    const sf::Color top(120, 240, 255, 0);
    beam[0].position = {m_position.x - beamWidth, m_position.y};
    beam[1].position = {m_position.x + beamWidth, m_position.y};
    beam[2].position = {m_position.x - beamWidth * 0.5f, m_position.y - beamHeight};
    beam[3].position = {m_position.x + beamWidth * 0.5f, m_position.y - beamHeight};
    beam[0].color = beam[1].color = base;
    beam[2].color = beam[3].color = top;
    target.draw(beam, sf::RenderStates(add));

    // 8 hạt phép thuật xoay quanh vòng
    for (int i = 0; i < 8; ++i) {
        const float a = m_time * 1.4f + kTwoPi * static_cast<float>(i) / 8.f;
        sf::CircleShape dot(3.5f);
        dot.setOrigin({3.5f, 3.5f});
        dot.setPosition({m_position.x + std::cos(a) * m_radius * 0.95f, m_position.y + std::sin(a) * m_radius * 0.95f * kGroundSquash});
        dot.setFillColor(sf::Color(220, 255, 255, 230));
        target.draw(dot, sf::RenderStates(add));
    }

    // Tia lấp lánh bay lên (vị trí tính từ thời gian nên không cần lưu từng hạt)
    for (int i = 0; i < 7; ++i) {
        const float cycle = std::fmod(m_time * 0.45f + static_cast<float>(i) / 7.f, 1.f);  // 0..1
        const float x = m_position.x + std::sin(static_cast<float>(i) * 2.3f + m_time) * m_radius * 0.6f;
        const float y = m_position.y - cycle * 120.f;
        sf::CircleShape spark(2.5f);
        spark.setOrigin({2.5f, 2.5f});
        spark.setPosition({x, y});
        spark.setFillColor(sf::Color(200, 255, 255, static_cast<std::uint8_t>(220.f * (1.f - cycle))));
        target.draw(spark, sf::RenderStates(add));
    }
}
