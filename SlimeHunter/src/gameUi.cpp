// GameUi.cpp - Toàn bộ phần VẼ của lớp Game (thế giới, HUD, các màn hình, Leaderboard, Shop, PowerUpSelect).

#include "Game.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace {
const sf::Color kGold(255, 232, 120);
const sf::Color kCyan(140, 240, 255);
}  // namespace

void Game::render() {
    m_window.clear(getLevelConfig(m_levelId).clearColor);

    drawWorld();
    switch (m_state) {
        case GameState::Login:
            drawLoginScreen();
            break;
        case GameState::Menu:
            drawMenu();
            drawToastAndHints();
            break;
        case GameState::Playing:
            drawHud();
            drawToastAndHints();
            break;
        case GameState::Paused:
            drawHud();
            drawPauseOverlay();
            break;
        case GameState::LevelComplete:
            drawHud();
            drawLevelCompleteOverlay();
            break;
        case GameState::GameOver:
            drawHud();
            drawGameOverOverlay();
            break;
        case GameState::Victory:
            drawHud();
            drawVictoryOverlay();
            break;
        case GameState::Leaderboard:
            drawLeaderboard();
            drawToastAndHints();
            break;
        case GameState::Shop:
            drawShop();
            drawToastAndHints();
            break;
        case GameState::PowerUpSelect:
            drawHud();
            drawPowerUpSelectOverlay();
            drawToastAndHints();
            break;
    }
    drawFade();

    if (!m_pendingScreenshot.empty()) {
        sf::Texture capture;
        if (capture.resize(m_window.getSize())) {
            capture.update(m_window);
            if (!capture.copyToImage().saveToFile(m_pendingScreenshot)) std::cerr << "Khong luu duoc anh chup\n";
        }
        m_pendingScreenshot.clear();
    }
    m_window.display();
}

// ============================================================================
//  Thế giới
// ============================================================================
void Game::drawWorld() {
    m_window.setView(m_camera);
    m_map.draw(m_window);

    const bool inLevel = (m_state != GameState::Login && m_state != GameState::Menu &&
                          m_state != GameState::Leaderboard && m_state != GameState::Shop);
    if (inLevel) m_gate.draw(m_window);

    // Vẽ Items trên mặt đất
    for (const auto& item : m_items) {
        item->draw(m_window);
    }

    struct DrawItem {
        float y;
        const Monster* monster;
    };
    std::vector<DrawItem> items;
    items.reserve(m_monsters.size() + 1);
    items.push_back({m_player.getPosition().y, nullptr});
    for (const std::unique_ptr<Monster>& monster : m_monsters) items.push_back({monster->getPosition().y, monster.get()});
    std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) { return a.y < b.y; });

    m_player.drawShadow(m_window);
    for (const std::unique_ptr<Monster>& monster : m_monsters) monster->drawShadow(m_window);
    for (const DrawItem& item : items) {
        if (item.monster == nullptr) m_player.draw(m_window);
        else item.monster->draw(m_window);
    }

    // Vẽ Đạn bay (Projectiles)
    for (const auto& proj : m_projectiles) {
        proj.draw(m_window);
    }

    if (inLevel) {
        m_effects.drawWorld(m_window, m_assets.hasFont ? &m_assets.font : nullptr);
        if (m_phase == LevelPhase::GateOpen && m_state == GameState::Playing) drawGateArrow();
    }
    const sf::Vector2f viewSize = m_camera.getSize();
    m_effects.drawAmbient(m_window, sf::FloatRect(m_camera.getCenter() - viewSize / 2.f, viewSize));

    if (m_debug) drawDebug();
}

void Game::drawGateArrow() {
    const sf::Vector2f playerPos = m_player.getPosition();
    const sf::Vector2f gatePos = m_gate.getPosition();
    const sf::Vector2f dir = util::normalized(gatePos - playerPos);
    if (util::distance(playerPos, gatePos) < 220.f) return;

    const sf::Vector2f center = m_camera.getCenter();
    const sf::Vector2f halfSize = m_camera.getSize() * 0.44f;
    sf::Vector2f arrowPos = center + sf::Vector2f(dir.x * halfSize.x, dir.y * halfSize.y);

    sf::CircleShape arrow(16.f, 3);
    arrow.setOrigin({16.f, 16.f});
    arrow.setPosition(arrowPos);
    arrow.setRotation(sf::degrees(std::atan2(dir.y, dir.x) * 57.2957795f + 90.f));
    arrow.setFillColor(sf::Color(100, 255, 230, 210));
    arrow.setOutlineColor(sf::Color(20, 50, 45, 220));
    arrow.setOutlineThickness(2.f);
    m_window.draw(arrow);
}

void Game::drawDebug() {
    // 1. Hộp va chạm thân người chơi (Player Body Hitbox)
    const sf::FloatRect pHitbox = m_player.getHitbox();
    sf::RectangleShape pBox(pHitbox.size);
    pBox.setPosition(pHitbox.position);
    pBox.setFillColor(sf::Color(0, 255, 180, 45));
    pBox.setOutlineColor(sf::Color(0, 255, 200));
    pBox.setOutlineThickness(2.f);
    m_window.draw(pBox);

    // Vòng tròn chân người chơi (Foot collision circle)
    sf::CircleShape foot(m_player.getFootRadius());
    foot.setOrigin({m_player.getFootRadius(), m_player.getFootRadius()});
    foot.setPosition(m_player.getPosition());
    foot.setFillColor(sf::Color(0, 200, 255, 30));
    foot.setOutlineColor(sf::Color::Cyan);
    foot.setOutlineThickness(1.5f);
    m_window.draw(foot);

    // Hộp va chạm đòn chém tấn công (Slash Hitbox)
    const sf::FloatRect pAtk = m_player.getAttackHitbox();
    if (pAtk.size.x > 0.f && pAtk.size.y > 0.f) {
        sf::RectangleShape atkBox(pAtk.size);
        atkBox.setPosition(pAtk.position);
        atkBox.setFillColor(sf::Color(255, 80, 80, 50));
        atkBox.setOutlineColor(sf::Color(255, 90, 80));
        atkBox.setOutlineThickness(2.f);
        m_window.draw(atkBox);
    }

    // 2. Hộp va chạm quái vật (Monsters)
    for (const auto& m : m_monsters) {
        if (m->isDead()) continue;
        const sf::FloatRect mHitbox = m->getHitbox();
        sf::RectangleShape mBox(mHitbox.size);
        mBox.setPosition(mHitbox.position);
        mBox.setFillColor(sf::Color(255, 60, 60, 40));
        mBox.setOutlineColor(sf::Color(255, 200, 60));
        mBox.setOutlineThickness(2.f);
        m_window.draw(mBox);

        sf::CircleShape mFoot(m->getFootRadius());
        mFoot.setOrigin({m->getFootRadius(), m->getFootRadius()});
        mFoot.setPosition(m->getPosition());
        mFoot.setFillColor(sf::Color(255, 120, 60, 25));
        mFoot.setOutlineColor(sf::Color(255, 120, 60));
        mFoot.setOutlineThickness(1.5f);
        m_window.draw(mFoot);
    }

    // 3. Vùng va chạm đạn bay (Projectiles)
    for (const auto& proj : m_projectiles) {
        sf::CircleShape projCircle(proj.getRadius());
        projCircle.setOrigin({proj.getRadius(), proj.getRadius()});
        projCircle.setPosition(proj.getPosition());
        projCircle.setFillColor(sf::Color(255, 0, 255, 55));
        projCircle.setOutlineColor(sf::Color::Magenta);
        projCircle.setOutlineThickness(1.5f);
        m_window.draw(projCircle);
    }

    // 4. Vùng thu thập vật phẩm rơi (Items)
    for (const auto& item : m_items) {
        constexpr float itemRadius = 16.f;
        sf::CircleShape itemCircle(itemRadius);
        itemCircle.setOrigin({itemRadius, itemRadius});
        itemCircle.setPosition(item->getPosition());
        itemCircle.setFillColor(sf::Color(255, 215, 0, 50));
        itemCircle.setOutlineColor(sf::Color(255, 230, 80));
        itemCircle.setOutlineThickness(1.5f);
        m_window.draw(itemCircle);
    }

    // 5. Vùng va chạm cổng thoát (Exit Gate)
    if (m_state == GameState::Playing || m_phase == LevelPhase::GateOpen) {
        sf::CircleShape gateZone(m_gate.getRadius());
        gateZone.setOrigin({m_gate.getRadius(), m_gate.getRadius()});
        gateZone.setPosition(m_gate.getPosition());
        gateZone.setFillColor(sf::Color(50, 255, 100, 30));
        gateZone.setOutlineColor(sf::Color(80, 255, 140));
        gateZone.setOutlineThickness(2.f);
        m_window.draw(gateZone);
    }
}

// ============================================================================
//  Tiện ích vẽ
// ============================================================================
std::string Game::formatTime(float seconds) {
    const int total = static_cast<int>(seconds);
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", total / 60, total % 60);
    return buffer;
}

void Game::drawText(const std::string& str, unsigned size, sf::Vector2f pos, sf::Color color, Align align) {
    if (!m_assets.hasFont) return;
    sf::Text text(m_assets.font, util::utf8(str), size);
    text.setFillColor(color);
    text.setOutlineColor(sf::Color(12, 18, 14, static_cast<std::uint8_t>(235.f * static_cast<float>(color.a) / 255.f)));
    text.setOutlineThickness(std::max(2.f, static_cast<float>(size) / 14.f));
    const sf::FloatRect bounds = text.getLocalBounds();
    float originX = bounds.position.x;
    if (align == Align::Center) originX += bounds.size.x / 2.f;
    else if (align == Align::Right) originX += bounds.size.x;
    text.setOrigin({originX, bounds.position.y});
    text.setPosition(pos);
    m_window.draw(text);
}

void Game::drawBar(sf::Vector2f pos, sf::Vector2f size, float ratio, sf::Color fill) {
    sf::RectangleShape back(size);
    back.setPosition(pos);
    back.setFillColor(sf::Color(20, 24, 20, 220));
    back.setOutlineColor(sf::Color(240, 240, 230, 230));
    back.setOutlineThickness(2.f);
    m_window.draw(back);

    sf::RectangleShape bar({size.x * util::clampf(ratio, 0.f, 1.f), size.y});
    bar.setPosition(pos);
    bar.setFillColor(fill);
    m_window.draw(bar);
}

void Game::drawDim(sf::Color color) {
    sf::RectangleShape shade(m_uiView.getSize());
    shade.setFillColor(color);
    m_window.draw(shade);
}

void Game::drawFade() {
    if (m_fadeAlpha <= 0.f) return;
    m_window.setView(m_uiView);
    drawDim(sf::Color(0, 0, 0, static_cast<std::uint8_t>(255.f * util::clampf(m_fadeAlpha, 0.f, 1.f))));
}

void Game::drawToastAndHints() {
    m_window.setView(m_uiView);
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;
    if (m_toastTimer > 0.f && !m_toast.empty()) {
        const float fade = util::clampf(m_toastTimer / 0.5f, 0.f, 1.f);
        drawText(m_toast, 26, {w / 2.f, (m_state == GameState::Menu) ? h * 0.88f : 190.f}, sf::Color(255, 255, 255, static_cast<std::uint8_t>(255.f * fade)), Align::Center);
    }
    if (m_state == GameState::Playing && m_gateHintTimer > 0.f && m_phase == LevelPhase::Fighting) {
        drawText("Hãy tiêu diệt toàn bộ quái để mở cổng (còn " + std::to_string(monstersRemaining()) + ")", 26, {w / 2.f, h * 0.72f},
                 sf::Color(255, 190, 90), Align::Center);
    }
}

// ============================================================================
//  HUD
// ============================================================================
void Game::drawHud() {
    m_window.setView(m_uiView);
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;
    const LevelConfig& config = getLevelConfig(m_levelId);

    // --- Góc trái trên: Máu, Exp, Tên ---
    drawText("Lv " + std::to_string(m_player.getLevel()), 22, {18.f, 10.f}, kGold);
    drawText(m_user.has_value() ? m_user->username : "Khách (không lưu)", 16, {100.f, 15.f}, sf::Color(200, 210, 200));

    const float hpRatio = m_player.getHp() / m_player.getMaxHp();
    const sf::Color hpColor = hpRatio > 0.5f ? sf::Color(90, 210, 90) : (hpRatio > 0.25f ? sf::Color(240, 190, 50) : sf::Color(225, 60, 55));
    drawBar({18.f, 42.f}, {280.f, 22.f}, hpRatio, hpColor);
    drawText("HP " + std::to_string(static_cast<int>(std::ceil(m_player.getHp()))) + " / " + std::to_string(static_cast<int>(m_player.getMaxHp())), 16,
             {158.f, 44.f}, sf::Color::White, Align::Center);

    drawBar({18.f, 70.f}, {280.f, 8.f}, static_cast<float>(m_player.getExp()) / static_cast<float>(m_player.getExpToNext()), sf::Color(90, 170, 255));

    // --- Thanh Cooldown LƯỚT (DASH) ---
    const float dashRatio = 1.0f - m_player.getDashCooldownRatio();
    drawBar({18.f, 84.f}, {130.f, 12.f}, dashRatio, dashRatio >= 0.99f ? sf::Color(80, 220, 255) : sf::Color(160, 160, 170));
    drawText("SPACE: LƯỚT", 12, {83.f, 84.f}, sf::Color::White, Align::Center);

    // --- Vũ khí hiện tại ---
    std::string weaponName = m_player.getWeapon() ? m_player.getWeapon()->getName() : "Tay Không";
    drawText("Vũ khí: " + weaponName + "  [1: Gậy | 2: Phép | 3: Boomerang]", 15, {18.f, 102.f}, sf::Color(255, 230, 140));

    // --- Thông tin màn chơi ---
    drawText("Màn " + std::to_string(m_levelId) + "/" + std::to_string(kLevelCount) + ": " + config.name, 17, {18.f, 124.f});
    if (m_phase == LevelPhase::GateOpen || m_phase == LevelPhase::Completed) drawText("Cổng đã MỞ - hãy đi tới cổng sáng!", 17, {18.f, 146.f}, kCyan);
    else drawText("Quái còn lại: " + std::to_string(monstersRemaining()), 17, {18.f, 146.f}, sf::Color(255, 205, 140));

    // --- Góc phải trên: Điểm, Vàng, Hạ gục, Âm thanh ---
    drawText("ĐIỂM  " + std::to_string(m_score), 28, {w - 20.f, 10.f}, sf::Color(255, 240, 160), Align::Right);
    drawText("Vàng: " + std::to_string(m_player.getCoins()) + " G", 20, {w - 20.f, 44.f}, sf::Color(255, 215, 60), Align::Right);
    drawText("Hạ gục: " + std::to_string(m_kills) + "  •  " + formatTime(m_survivalTime), 18, {w - 20.f, 72.f}, sf::Color::White, Align::Right);
    drawText(m_audio.isMuted() ? "[M] Âm: TẮT" : "[M] Âm: BẬT", 15, {w - 20.f, 96.f}, m_audio.isMuted() ? sf::Color(255, 120, 120) : sf::Color(140, 255, 160), Align::Right);

    // --- Thanh máu Boss ---
    if (const Monster* boss = findBoss()) {
        drawText("BOSS - SLIME KING", 22, {w / 2.f, 10.f}, sf::Color(255, 120, 90), Align::Center);
        drawBar({w / 2.f - 220.f, 42.f}, {440.f, 18.f}, boss->getHp() / boss->getMaxHp(), sf::Color(230, 70, 50));
    }
    if (m_levelUpTimer > 0.f) {
        drawText("LÊN CẤP!  Lv " + std::to_string(m_player.getLevel()), 34, {w / 2.f, 85.f}, sf::Color(255, 230, 100), Align::Center);
    }

    // --- Hướng dẫn phím chân màn hình ---
    drawText("WASD: đi   •   Chuột trái/J: đánh   •   Space/Phải: Lướt   •   1/2/3: Vũ khí   •   [B]: BXH   •   [M]: Âm thanh   •   [F1/H]: Debug   •   Esc: Dừng",
             15, {w / 2.f, h - 24.f}, sf::Color(235, 235, 225, 200), Align::Center);

    if (m_debug) {
        drawText("DEBUG [F1/H]: BẬT  FPS " + std::to_string(static_cast<int>(m_fps)) + "  Quái: " + std::to_string(m_monsters.size()) +
                 "  Đạn: " + std::to_string(m_projectiles.size()) + "  Items: " + std::to_string(m_items.size()),
                 15, {18.f, 168.f}, sf::Color::Cyan);
    }
}

// ============================================================================
//  Các màn hình
// ============================================================================
void Game::drawLoginScreen() {
    m_window.setView(m_uiView);
    const float w = m_uiView.getSize().x;
    drawDim(sf::Color(8, 14, 10, 175));

    drawText("SLIME HUNTER", 72, {w / 2.f, 40.f}, sf::Color(150, 255, 90), Align::Center);

    sf::RectangleShape panel({580.f, 350.f});
    panel.setPosition({(w - 580.f) / 2.f, 160.f});
    panel.setFillColor(sf::Color(14, 24, 18, 225));
    panel.setOutlineColor(kGold);
    panel.setOutlineThickness(3.f);
    m_window.draw(panel);

    drawText(m_registerMode ? "ĐĂNG KÝ TÀI KHOẢN" : "ĐĂNG NHẬP", 30, {w / 2.f, 175.f}, kGold, Align::Center);

    const bool blink = static_cast<int>(m_menuTime * 2.f) % 2 == 0;
    const float boxX = (w - 480.f) / 2.f;
    const sf::Vector2f mousePos = m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_uiView);

    auto drawField = [&](const char* label, const std::string& value, float y, bool active, bool isPassword = false) {
        drawText(label, 18, {boxX, y}, sf::Color(210, 220, 210));
        sf::FloatRect fieldBounds({boxX, y + 28.f}, {480.f, 42.f});
        const bool hovered = fieldBounds.contains(mousePos);

        sf::RectangleShape box(fieldBounds.size);
        box.setPosition(fieldBounds.position);
        box.setFillColor(active ? sf::Color(10, 18, 14, 245) : (hovered ? sf::Color(16, 26, 20, 235) : sf::Color(6, 10, 8, 235)));
        box.setOutlineColor(active ? kGold : (hovered ? sf::Color(180, 210, 180) : sf::Color(120, 130, 120)));
        box.setOutlineThickness(active ? 3.f : (hovered ? 2.5f : 1.5f));
        m_window.draw(box);

        std::string displayText = value;
        const std::size_t maxVisible = isPassword ? 23 : 26;
        if (displayText.size() > maxVisible) {
            displayText = displayText.substr(displayText.size() - maxVisible);
        }
        drawText(displayText + ((active && blink) ? "|" : ""), 24, {boxX + 12.f, y + 33.f});
    };
    drawField("Tên đăng nhập", m_loginUser, 225.f, m_loginField == 0, false);

    const std::string passDisplay = m_showPassword ? m_loginPass : std::string(m_loginPass.size(), '*');
    drawField("Mật khẩu", passDisplay, 305.f, m_loginField == 1, true);

    // Nút Con Mắt (Ẩn / Hiện mật khẩu)
    const sf::Vector2f eyeCenter{boxX + 480.f - 24.f, 305.f + 28.f + 21.f};
    const sf::FloatRect eyeBounds({boxX + 480.f - 40.f, 333.f + 6.f}, {34.f, 30.f});
    const bool eyeHovered = eyeBounds.contains(mousePos);
    drawEyeIcon(eyeCenter, m_showPassword, eyeHovered);

    if (eyeHovered) {
        drawText(m_showPassword ? "Ẩn mật khẩu" : "Hiện mật khẩu", 14, {eyeCenter.x, eyeCenter.y - 25.f}, sf::Color(255, 235, 120), Align::Center);
    }

    if (!m_loginMessage.empty()) {
        drawText(m_loginMessage, 17, {w / 2.f, 390.f}, m_loginMessageIsError ? sf::Color(255, 110, 100) : sf::Color(140, 255, 160), Align::Center);
    }

    const std::string submitHint = m_registerMode
        ? "Enter: Tạo tài khoản"
        : (m_loginField == 0 ? "Enter: Tiếp tục sang ô Mật khẩu" : "Enter: Đăng nhập");
    drawText(submitHint, 21, {w / 2.f, 425.f}, sf::Color(255, 235, 120), Align::Center);
    drawText(std::string("↑ ↓ hoặc Tab / Click: đổi ô   •   F2: ") + (m_registerMode ? "Quay lại đăng nhập" : "Tạo tài khoản mới") + "   •   F3: Chơi khách", 16,
             {w / 2.f, 465.f}, sf::Color(230, 230, 215), Align::Center);
}

void Game::drawEyeIcon(sf::Vector2f center, bool open, bool hovered) {
    // 1. Khung nền nút con mắt
    sf::RectangleShape btnBox({32.f, 28.f});
    btnBox.setOrigin({16.f, 14.f});
    btnBox.setPosition(center);
    btnBox.setFillColor(hovered ? sf::Color(40, 60, 50, 230) : sf::Color(16, 26, 20, 190));
    btnBox.setOutlineColor(hovered ? sf::Color(255, 235, 120) : sf::Color(80, 105, 90));
    btnBox.setOutlineThickness(hovered ? 1.8f : 1.0f);
    m_window.draw(btnBox);

    // 2. Viền ngoài hình quả hạnh nhân (Almond eye contour)
    constexpr int kEyePoints = 16;
    sf::ConvexShape eye(kEyePoints);
    constexpr float halfW = 10.5f;
    constexpr float halfH = 5.8f;
    for (int i = 0; i <= 8; ++i) {
        const float t = -1.f + (2.f * static_cast<float>(i)) / 8.f;
        const float x = t * halfW;
        const float y = -halfH * (1.f - t * t);
        eye.setPoint(i, sf::Vector2f(x, y));
    }
    for (int i = 1; i < 8; ++i) {
        const float t = 1.f - (2.f * static_cast<float>(i)) / 8.f;
        const float x = t * halfW;
        const float y = halfH * (1.f - t * t);
        eye.setPoint(8 + i, sf::Vector2f(x, y));
    }
    eye.setPosition(center);
    eye.setFillColor(hovered ? sf::Color(24, 38, 30, 230) : sf::Color(10, 18, 14, 210));
    eye.setOutlineColor(open ? (hovered ? sf::Color(130, 255, 200) : sf::Color(90, 210, 170))
                             : (hovered ? sf::Color(255, 210, 160) : sf::Color(160, 175, 170)));
    eye.setOutlineThickness(1.6f);
    m_window.draw(eye);

    // 3. Tròng mắt (Pupil / Iris)
    sf::CircleShape iris(3.4f);
    iris.setOrigin({3.4f, 3.4f});
    iris.setPosition(center);
    if (open) {
        iris.setFillColor(hovered ? sf::Color(110, 240, 255) : sf::Color(70, 205, 235));
        m_window.draw(iris);

        // Đồng tử đen ở giữa
        sf::CircleShape pupil(1.8f);
        pupil.setOrigin({1.8f, 1.8f});
        pupil.setPosition(center);
        pupil.setFillColor(sf::Color(12, 16, 20));
        m_window.draw(pupil);

        // Đốm sáng phản chiếu (Specular reflection)
        sf::CircleShape shine(0.9f);
        shine.setOrigin({0.9f, 0.9f});
        shine.setPosition({center.x - 1.1f, center.y - 1.1f});
        shine.setFillColor(sf::Color::White);
        m_window.draw(shine);
    } else {
        iris.setFillColor(sf::Color(120, 130, 130, 180));
        m_window.draw(iris);

        // Vạch gạch chéo đỏ cam khi đóng / ẩn mật khẩu
        sf::RectangleShape slash({24.f, 2.2f});
        slash.setOrigin({12.f, 1.1f});
        slash.setPosition(center);
        slash.setRotation(sf::degrees(38.f));
        slash.setFillColor(hovered ? sf::Color(255, 100, 90) : sf::Color(220, 85, 80));
        m_window.draw(slash);
    }
}

void Game::drawMenu() {
    m_window.setView(m_uiView);
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;
    drawDim(sf::Color(10, 20, 14, 150));

    drawText("SLIME HUNTER", 76, {w / 2.f, h * 0.05f}, sf::Color(150, 255, 90), Align::Center);
    if (m_debug) {
        drawText("[DEBUG HỘP VA CHẠM: BẬT]", 20, {w / 2.f, h * 0.14f}, sf::Color(100, 255, 230), Align::Center);
    }
    drawText(m_user.has_value() ? "Xin chào, " + m_user->username : "Chơi khách - tiến trình không được lưu", 24, {w / 2.f, h * 0.20f},
             sf::Color(240, 240, 220), Align::Center);

    const LevelConfig& config = getLevelConfig(m_selectedLevel);
    const int maxSelectable = std::min(m_progress.maxLevelUnlocked, kLevelCount);
    drawText(std::string(m_selectedLevel > 1 ? "<   " : "    ") + "Màn " + std::to_string(m_selectedLevel) + "/" + std::to_string(kLevelCount) + ": " + config.name +
                 (m_selectedLevel < maxSelectable ? "   >" : "    "),
             32, {w / 2.f, h * 0.29f}, kGold, Align::Center);
    drawText("Đã mở khóa: " + std::string(m_progress.maxLevelUnlocked > kLevelCount ? "tất cả" : std::to_string(m_progress.maxLevelUnlocked)) + "/" + std::to_string(kLevelCount) +
                 " màn   •   ← → hoặc A D để chọn màn",
             16, {w / 2.f, h * 0.29f + 44.f}, sf::Color(210, 220, 210), Align::Center);

    drawText("Máu " + std::to_string(m_progress.playerMaxHp) + "    Sát thương " + std::to_string(m_progress.playerDamage) + "    Chém: " +
                 m_player.getSlashProfile().name,
             20, {w / 2.f, h * 0.42f}, sf::Color(170, 210, 255), Align::Center);
    drawText("Tổng hạ gục: " + std::to_string(m_progress.totalKills) + "    Tổng điểm tích lũy: " + std::to_string(m_progress.totalScore), 18, {w / 2.f, h * 0.42f + 30.f},
             sf::Color(255, 230, 140), Align::Center);

    const bool blinkOn = static_cast<int>(m_menuTime * 1.6f) % 2 == 0;
    if (blinkOn) drawText("Nhấn ENTER hoặc SPACE để BẮT ĐẦU CHƠI", 28, {w / 2.f, h * 0.58f}, sf::Color(255, 235, 120), Align::Center);

    // Các nút chức năng nổi bật với viền nút và hiệu ứng hover chuột
    const float btnY = h * 0.69f;
    const sf::Vector2f mousePos = m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_uiView);

    const sf::FloatRect lbRect({w * 0.32f - 165.f, btnY - 26.f}, {330.f, 52.f});
    const bool lbHover = lbRect.contains(mousePos);
    sf::RectangleShape lbBox(lbRect.size);
    lbBox.setPosition(lbRect.position);
    lbBox.setFillColor(lbHover ? sf::Color(16, 36, 52, 230) : sf::Color(10, 20, 32, 200));
    lbBox.setOutlineColor(lbHover ? sf::Color(140, 240, 255) : sf::Color(70, 140, 180));
    lbBox.setOutlineThickness(lbHover ? 3.f : 1.5f);
    m_window.draw(lbBox);
    drawText("[B] BẢNG XẾP HẠNG TOP ĐIỂM", 22, {w * 0.32f, btnY - 14.f}, lbHover ? sf::Color(180, 245, 255) : sf::Color(120, 230, 255), Align::Center);

    const sf::FloatRect shopRect({w * 0.68f - 165.f, btnY - 26.f}, {330.f, 52.f});
    const bool shopHover = shopRect.contains(mousePos);
    sf::RectangleShape shopBox(shopRect.size);
    shopBox.setPosition(shopRect.position);
    shopBox.setFillColor(shopHover ? sf::Color(52, 36, 16, 230) : sf::Color(32, 20, 10, 200));
    shopBox.setOutlineColor(shopHover ? sf::Color(255, 220, 100) : sf::Color(180, 130, 60));
    shopBox.setOutlineThickness(shopHover ? 3.f : 1.5f);
    m_window.draw(shopBox);
    drawText("[S] CỬA HÀNG NÂNG CẤP", 22, {w * 0.68f, btnY - 14.f}, shopHover ? sf::Color(255, 240, 140) : sf::Color(255, 190, 80), Align::Center);

    const float y = h * 0.79f;
    drawText("1/2/3: đổi vũ khí      Space: Lướt né      Shift: chạy nhanh      M: Bật/Tắt âm      F11: Toàn màn hình", 18, {w / 2.f, y}, sf::Color::White, Align::Center);
    drawText("Esc: thoát      L: đăng xuất      F1 / H: xem hộp va chạm debug", 18, {w / 2.f, y + 28.f},
             m_debug ? sf::Color(100, 255, 230) : sf::Color::White, Align::Center);
}

void Game::drawLeaderboard() {
    m_window.setView(m_uiView);
    drawDim(sf::Color(10, 16, 24, 230));
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;

    drawText("BẢNG XẾP HẠNG NGƯỜI CHƠI", 44, {w / 2.f, 60.f}, sf::Color(140, 230, 255), Align::Center);
    drawText("Top người chơi có điểm tích lũy cao nhất (từ data/player_progress.csv)", 18, {w / 2.f, 115.f}, sf::Color(200, 210, 220), Align::Center);

    sf::RectangleShape panel({820.f, 380.f});
    panel.setPosition({(w - 820.f) / 2.f, 160.f});
    panel.setFillColor(sf::Color(15, 22, 32, 240));
    panel.setOutlineColor(sf::Color(100, 190, 255));
    panel.setOutlineThickness(2.f);
    m_window.draw(panel);

    // Tiêu đề cột
    const float startY = 185.f;
    drawText("HẠNG", 20, {w / 2.f - 350.f, startY}, kGold);
    drawText("TÊN TÀI KHOẢN", 20, {w / 2.f - 200.f, startY}, kGold);
    drawText("MÀN ĐẠT ĐƯỢC", 20, {w / 2.f + 50.f, startY}, kGold);
    drawText("ĐIỂM SỐ", 20, {w / 2.f + 250.f, startY}, kGold);

    float rowY = startY + 45.f;
    int rank = 1;
    for (const auto& entry : m_topScores) {
        sf::Color rowColor = (rank == 1) ? sf::Color(255, 230, 80) : ((rank == 2) ? sf::Color(220, 220, 230) : ((rank == 3) ? sf::Color(240, 160, 90) : sf::Color::White));
        drawText(std::string("#") + std::to_string(rank), 20, {w / 2.f - 340.f, rowY}, rowColor);
        drawText(entry.username, 20, {w / 2.f - 200.f, rowY}, rowColor);
        drawText("Màn " + std::to_string(std::min(entry.maxLevel, kLevelCount)), 20, {w / 2.f + 80.f, rowY}, rowColor);
        drawText(std::to_string(entry.score), 22, {w / 2.f + 250.f, rowY}, sf::Color(255, 240, 140));
        rowY += 48.f;
        ++rank;
    }

    if (m_topScores.empty()) {
        drawText("Chưa có bản ghi điểm nào.", 22, {w / 2.f, 320.f}, sf::Color(180, 180, 180), Align::Center);
    }

    const std::string backHint = (m_leaderboardPrevState == GameState::Playing || m_leaderboardPrevState == GameState::Paused)
        ? "Nhấn ESC, ENTER, SPACE, [B] hoặc Click chuột để quay lại Trận chiến"
        : "Nhấn ESC, ENTER, SPACE, [B] hoặc Click chuột để quay lại Menu";
    drawText(backHint, 24, {w / 2.f, h - 80.f}, sf::Color(255, 235, 120), Align::Center);
}

void Game::drawShop() {
    m_window.setView(m_uiView);
    drawDim(sf::Color(24, 16, 10, 235));
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;

    drawText("CỬA HÀNG NÂNG CẤP VĨNH VIỄN", 44, {w / 2.f, 50.f}, sf::Color(255, 215, 90), Align::Center);
    drawText("Dùng điểm tích lũy để mở khóa chỉ số khởi đầu (Lưu vĩnh viễn vào CSV)", 18, {w / 2.f, 105.f}, sf::Color(240, 230, 210), Align::Center);
    drawText("Điểm tích lũy hiện có: " + std::to_string(m_progress.totalScore) + " ĐIỂM", 26, {w / 2.f, 140.f}, sf::Color(100, 255, 140), Align::Center);

    float cardY = 190.f;
    const sf::Vector2f mousePos = m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_uiView);
    for (int i = 0; i < static_cast<int>(m_shopItems.size()); ++i) {
        const auto& item = m_shopItems[i];
        const float boxW = 750.f;
        const float boxH = 75.f;
        const sf::FloatRect itemRect({(w - boxW) / 2.f, cardY}, {boxW, boxH});
        const bool hovered = itemRect.contains(mousePos);
        const bool selected = (i == m_selectedShopIndex) || hovered;

        sf::RectangleShape box({boxW, boxH});
        box.setPosition(itemRect.position);
        box.setFillColor(selected ? sf::Color(45, 30, 18, 250) : sf::Color(20, 14, 10, 230));
        box.setOutlineColor(selected ? (hovered ? sf::Color(255, 230, 130) : kGold) : sf::Color(100, 80, 60));
        box.setOutlineThickness(selected ? 3.f : 1.5f);
        m_window.draw(box);

        drawText((selected ? "> " : "  ") + item.name, 22, {(w - 750.f) / 2.f + 25.f, cardY + 12.f}, selected ? kGold : sf::Color::White);
        drawText(item.desc, 16, {(w - 750.f) / 2.f + 45.f, cardY + 44.f}, sf::Color(200, 200, 190));

        if (item.purchased) {
            drawText("ĐÃ SỞ HỮU", 20, {(w + 750.f) / 2.f - 40.f, cardY + 24.f}, sf::Color(100, 255, 140), Align::Right);
        } else {
            drawText(std::to_string(item.cost) + " Điểm", 22, {(w + 750.f) / 2.f - 40.f, cardY + 24.f},
                     m_progress.totalScore >= item.cost ? sf::Color(255, 230, 120) : sf::Color(220, 90, 80), Align::Right);
        }
        cardY += 95.f;
    }

    drawText("Phím Mũi Tên Lên / Xuống hoặc W / S: Chọn món   •   ENTER / SPACE hoặc Click: Mua", 19, {w / 2.f, h - 90.f}, sf::Color::White, Align::Center);

    const sf::FloatRect backBtn({w / 2.f - 160.f, h - 70.f}, {320.f, 40.f});
    const bool backHover = backBtn.contains(mousePos);
    sf::RectangleShape backBox(backBtn.size);
    backBox.setPosition(backBtn.position);
    backBox.setFillColor(backHover ? sf::Color(65, 42, 20, 240) : sf::Color(35, 24, 15, 210));
    backBox.setOutlineColor(backHover ? sf::Color(255, 235, 120) : sf::Color(180, 140, 70));
    backBox.setOutlineThickness(backHover ? 2.5f : 1.5f);
    m_window.draw(backBox);
    drawText("ESC / Click: Quay lại Menu", 20, {w / 2.f, h - 56.f}, backHover ? sf::Color(255, 255, 180) : sf::Color(255, 235, 120), Align::Center);
}

void Game::drawPowerUpSelectOverlay() {
    m_window.setView(m_uiView);
    drawDim(sf::Color(10, 18, 14, 215));
    const float w = m_uiView.getSize().x;

    const auto layout = getPowerUpCardLayout(m_uiView.getSize());
    const sf::Vector2f mousePos = m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_uiView);

    const float titleY = layout.cardY - 105.f;
    const float subY = layout.cardY - 50.f;
    drawText("LÊN CẤP! CHỌN 1 NÂNG CẤP ROGUE-LITE", 42, {w / 2.f, titleY}, sf::Color(255, 235, 90), Align::Center);
    drawText("Chọn 1 kỹ năng để cường hóa nhân vật trong phiên chơi này:", 20, {w / 2.f, subY}, sf::Color(220, 230, 220), Align::Center);

    for (int i = 0; i < static_cast<int>(m_powerUpChoices.size()); ++i) {
        const auto& card = m_powerUpChoices[i];
        const sf::FloatRect bounds = layout.getBounds(i);
        const bool hovered = bounds.contains(mousePos);

        sf::RectangleShape box(bounds.size);
        box.setPosition(bounds.position);
        box.setFillColor(hovered ? sf::Color(28, 48, 38, 250) : sf::Color(16, 26, 20, 245));
        box.setOutlineColor(hovered ? sf::Color(130, 255, 200) : kGold);
        box.setOutlineThickness(hovered ? 4.f : 2.5f);
        m_window.draw(box);

        const float x = bounds.position.x;
        const float cardY = bounds.position.y;
        const float cardW = bounds.size.x;

        // Số phím
        sf::CircleShape keyCircle(18.f);
        keyCircle.setOrigin({18.f, 18.f});
        keyCircle.setPosition({x + 35.f, cardY + 35.f});
        keyCircle.setFillColor(hovered ? sf::Color(45, 80, 60) : sf::Color(30, 50, 40));
        keyCircle.setOutlineColor(hovered ? sf::Color(130, 255, 200) : kGold);
        keyCircle.setOutlineThickness(2.f);
        m_window.draw(keyCircle);
        drawText(std::to_string(i + 1), 20, {x + 35.f, cardY + 22.f}, hovered ? sf::Color(130, 255, 200) : kGold, Align::Center);

        // Icon
        drawText(card.icon, 24, {x + cardW - 45.f, cardY + 22.f}, sf::Color(140, 230, 255), Align::Center);

        // Tiêu đề
        drawText(card.title, 23, {x + cardW / 2.f, cardY + 80.f}, hovered ? sf::Color(255, 255, 180) : sf::Color(255, 240, 150), Align::Center);

        // Mô tả
        drawText(card.description, 17, {x + cardW / 2.f, cardY + 140.f}, sf::Color(220, 230, 225), Align::Center);

        drawText("Nhấn [" + std::to_string(i + 1) + "] hoặc Click để chọn", 16, {x + cardW / 2.f, cardY + 215.f},
                 hovered ? sf::Color(120, 255, 180) : sf::Color(150, 220, 170), Align::Center);
    }
}

void Game::drawPauseOverlay() {
    m_window.setView(m_uiView);
    drawDim(sf::Color(0, 0, 0, 150));
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;
    drawText("TẠM DỪNG", 68, {w / 2.f, h * 0.30f}, sf::Color::White, Align::Center);
    drawText(std::string("Màn ") + std::to_string(m_levelId) + ": " + getLevelConfig(m_levelId).name, 26, {w / 2.f, h * 0.30f + 90.f}, kGold, Align::Center);
    drawText("Esc / Enter: tiếp tục      Q: về menu (tự động lưu)", 26, {w / 2.f, h * 0.30f + 140.f}, sf::Color(230, 230, 210), Align::Center);
}

void Game::drawLevelCompleteOverlay() {
    m_window.setView(m_uiView);
    drawDim(sf::Color(0, 24, 10, 160));
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;
    drawText("LEVEL COMPLETED", 74, {w / 2.f, h * 0.10f}, sf::Color(130, 255, 140), Align::Center);
    drawText("Màn " + std::to_string(m_levelId) + ": " + getLevelConfig(m_levelId).name, 30, {w / 2.f, h * 0.10f + 100.f}, kGold, Align::Center);
    drawText("Hạ gục trong màn: " + std::to_string(m_levelKills), 22, {w / 2.f, h * 0.10f + 150.f}, sf::Color::White, Align::Center);
    float y = h * 0.43f;
    for (const std::string& line : m_rewardLines) {
        const bool unlock = line.rfind("MỞ KHÓA", 0) == 0;
        drawText(line, unlock ? 24 : 22, {w / 2.f, y}, unlock ? sf::Color(255, 215, 90) : sf::Color(235, 240, 235), Align::Center);
        y += 34.f;
    }
    if (m_levelCompleteTimer > 0.8f && static_cast<int>(m_menuTime * 1.6f) % 2 == 0)
        drawText("Nhấn ENTER để tiếp tục", 24, {w / 2.f, h * 0.88f}, sf::Color(255, 235, 120), Align::Center);
}

void Game::drawVictoryOverlay() {
    m_window.setView(m_uiView);
    drawDim(sf::Color(30, 24, 0, 165));
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;
    drawText("CHIẾN THẮNG!", 80, {w / 2.f, h * 0.08f}, sf::Color(255, 225, 90), Align::Center);
    drawText("Bạn đã hoàn thành cả " + std::to_string(kLevelCount) + " màn chơi", 28, {w / 2.f, h * 0.08f + 100.f}, sf::Color::White, Align::Center);
    drawText("Điểm lượt này: " + std::to_string(m_score) + "    Hạ gục: " + std::to_string(m_kills) + "    Sống sót: " + formatTime(m_survivalTime), 22,
             {w / 2.f, h * 0.08f + 148.f}, sf::Color(255, 240, 160), Align::Center);
    float y = h * 0.42f;
    for (const std::string& line : m_rewardLines) {
        const bool unlock = line.rfind("MỞ KHÓA", 0) == 0;
        drawText(line, unlock ? 24 : 22, {w / 2.f, y}, unlock ? sf::Color(255, 215, 90) : sf::Color(235, 240, 235), Align::Center);
        y += 34.f;
    }
    drawText("Nhấn ENTER để về Menu", 26, {w / 2.f, h * 0.88f}, sf::Color(255, 235, 120), Align::Center);
}

void Game::drawGameOverOverlay() {
    m_window.setView(m_uiView);
    drawDim(sf::Color(70, 0, 0, 160));
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;
    drawText("GAME OVER", 80, {w / 2.f, h * 0.16f}, sf::Color(255, 90, 80), Align::Center);
    drawText("Màn " + std::to_string(m_levelId) + ": " + getLevelConfig(m_levelId).name, 28, {w / 2.f, h * 0.16f + 100.f}, kGold, Align::Center);
    drawText("Điểm số: " + std::to_string(m_score), 40, {w / 2.f, h * 0.16f + 145.f}, sf::Color(255, 240, 160), Align::Center);
    drawText("Hạ gục: " + std::to_string(m_kills) + "    Cấp độ: " + std::to_string(m_player.getLevel()) + "    Sống sót: " + formatTime(m_survivalTime), 24,
             {w / 2.f, h * 0.16f + 205.f}, sf::Color::White, Align::Center);
    drawText("Nhấn R để chơi lại màn này      Esc: về menu", 28, {w / 2.f, h * 0.16f + 275.f}, sf::Color(255, 235, 120), Align::Center);
}
