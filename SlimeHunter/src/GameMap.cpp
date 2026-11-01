#include "GameMap.hpp"

#include "Assets.hpp"
#include "Utils.hpp"

#include <cmath>

GameMap::GameMap(const Assets& assets, int levelId)
    : m_background(assets.getLevelBackground(levelId)), m_overlay(m_overlayTexture) {
    loadLevel(assets, levelId);
}

void GameMap::loadLevel(const Assets& assets, int levelId) {
    const sf::Texture& backgroundTexture = assets.getLevelBackground(levelId);
    m_background.setTexture(backgroundTexture, true);  // true: đặt lại hình chữ nhật cắt theo kích thước texture mới
    m_size = sf::Vector2f(backgroundTexture.getSize());

    const sf::Image& collisionMask = assets.getLevelCollision(levelId);
    const sf::Vector2u maskSize = collisionMask.getSize();
    m_cellsW = static_cast<int>(maskSize.x);
    m_cellsH = static_cast<int>(maskSize.y);
    m_cellSize = m_size.x / static_cast<float>(m_cellsW);
    m_blocked.assign(static_cast<std::size_t>(m_cellsW) * static_cast<std::size_t>(m_cellsH), 0);

    // Đọc mặt nạ vào mảng, đồng thời dựng ảnh phủ màu đỏ để xem vùng cấm khi debug.
    sf::Image overlayImage(maskSize, sf::Color::Transparent);
    for (unsigned y = 0; y < maskSize.y; ++y) {
        for (unsigned x = 0; x < maskSize.x; ++x) {
            if (collisionMask.getPixel({x, y}).r > 128) {
                m_blocked[y * maskSize.x + x] = 1;
                overlayImage.setPixel({x, y}, sf::Color(255, 40, 40, 110));
            }
        }
    }
    if (m_overlayTexture.loadFromImage(overlayImage)) m_overlay.setTexture(m_overlayTexture, true);
    m_overlay.setScale({m_cellSize, m_cellSize});

    // Dựng lưới dẫn đường thô: một ô là "bị chặn" nếu một vòng tròn bán kính 14px đặt ở tâm ô không vừa.
    // Nhờ "phình" vật cản như vậy, đường BFS tìm ra luôn đủ rộng cho quái đi qua.
    m_navW = static_cast<int>(std::ceil(m_size.x / kNavCellSize));
    m_navH = static_cast<int>(std::ceil(m_size.y / kNavCellSize));
    m_navBlocked.assign(static_cast<std::size_t>(m_navW) * static_cast<std::size_t>(m_navH), 0);
    for (int y = 0; y < m_navH; ++y) {
        for (int x = 0; x < m_navW; ++x) {
            const sf::Vector2f center((static_cast<float>(x) + 0.5f) * kNavCellSize, (static_cast<float>(y) + 0.5f) * kNavCellSize);
            if (!isCircleFree(center, 14.f)) m_navBlocked[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_navW) + static_cast<std::size_t>(x)] = 1;
        }
    }
}

bool GameMap::isNavBlocked(int cx, int cy) const {
    if (cx < 0 || cy < 0 || cx >= m_navW || cy >= m_navH) return true;
    return m_navBlocked[static_cast<std::size_t>(cy) * static_cast<std::size_t>(m_navW) + static_cast<std::size_t>(cx)] != 0;
}

bool GameMap::isLineFree(sf::Vector2f a, sf::Vector2f b, float radius) const {
    const sf::Vector2f delta = b - a;
    const float length = util::length(delta);
    const int steps = static_cast<int>(length / 8.f) + 1;  // lấy mẫu mỗi ~8px dọc đoạn thẳng
    for (int i = 1; i <= steps; ++i) {
        const sf::Vector2f p = a + delta * (static_cast<float>(i) / static_cast<float>(steps));
        if (!isCircleFree(p, radius * 0.8f)) return false;
    }
    return true;
}

void GameMap::draw(sf::RenderTarget& target) const { target.draw(m_background); }

void GameMap::drawCollisionOverlay(sf::RenderTarget& target) const { target.draw(m_overlay); }

bool GameMap::isBlocked(sf::Vector2f worldPos) const {
    const int cx = static_cast<int>(std::floor(worldPos.x / m_cellSize));
    const int cy = static_cast<int>(std::floor(worldPos.y / m_cellSize));
    if (cx < 0 || cy < 0 || cx >= m_cellsW || cy >= m_cellsH) return true;  // ngoài bản đồ = cấm
    return m_blocked[static_cast<std::size_t>(cy) * static_cast<std::size_t>(m_cellsW) + static_cast<std::size_t>(cx)] != 0;
}

bool GameMap::isCircleFree(sf::Vector2f center, float radius) const {
    if (isBlocked(center)) return false;
    const int samples = 8;  // kiểm tra 8 điểm trên chu vi vòng tròn
    for (int i = 0; i < samples; ++i) {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(samples);
        const sf::Vector2f p(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
        if (isBlocked(p)) return false;
    }
    return true;
}

sf::Vector2f GameMap::moveAndSlide(sf::Vector2f position, sf::Vector2f delta, float radius) const {
    sf::Vector2f result = position;

    // Thử cả bước, rồi nửa bước, rồi 1/4 bước: nhờ vậy nhân vật tiến sát vật cản thay vì dừng cách xa một khoảng,
    // và không bị kẹt ở những góc hẹp (mặt nạ có độ phân giải 3px).
    for (const float scale : {1.f, 0.5f, 0.25f}) {
        const sf::Vector2f tryX(result.x + delta.x * scale, result.y);
        if (isCircleFree(tryX, radius)) {
            result = tryX;
            break;
        }
    }
    for (const float scale : {1.f, 0.5f, 0.25f}) {
        const sf::Vector2f tryY(result.x, result.y + delta.y * scale);
        if (isCircleFree(tryY, radius)) {
            result = tryY;
            break;
        }
    }

    // Vẫn không nhích được: thường là chân chạm đúng một GÓC vật cản (mặt nạ 3px nên có những chỗ "cấn" như vậy).
    // Thử dịch ngang 2-6px, nhưng CHỈ chấp nhận nếu sau khi dịch thì bước đi thẳng đã đi được.
    // Đẩy thẳng vào một bức tường phẳng thì điều kiện này không bao giờ thỏa => không bị "trôi" ngang oan.
    if (result == position && (delta.x != 0.f || delta.y != 0.f)) {
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        const sf::Vector2f side(-delta.y / length, delta.x / length);  // vuông góc với hướng đi
        for (const float offset : {2.f, -2.f, 4.f, -4.f, 6.f, -6.f}) {
            const sf::Vector2f shifted = position + side * offset;
            if (isCircleFree(shifted, radius) && isCircleFree(shifted + delta, radius)) {
                result = shifted;
                break;
            }
        }
    }
    return result;
}

sf::Vector2f GameMap::findFreeSpotNear(sf::Vector2f target, float radius) const {
    if (isCircleFree(target, radius)) return target;
    for (float ring = 12.f; ring < 1500.f; ring += 12.f) {
        for (int step = 0; step < 24; ++step) {
            const float angle = 6.2831853f * static_cast<float>(step) / 24.f;
            const sf::Vector2f p(target.x + std::cos(angle) * ring, target.y + std::sin(angle) * ring);
            if (isCircleFree(p, radius)) return p;
        }
    }
    return target;  // gần như không thể xảy ra với bản đồ này
}

bool GameMap::findRandomFreeSpot(float radius, sf::Vector2f& outPosition) const {
    for (int attempt = 0; attempt < 80; ++attempt) {
        const sf::Vector2f p(util::randomFloat(radius, m_size.x - radius), util::randomFloat(radius, m_size.y - radius));
        if (isCircleFree(p, radius)) {
            outPosition = p;
            return true;
        }
    }
    return false;
}
