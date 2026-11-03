#include "PathField.hpp"

#include "GameMap.hpp"
#include "Utils.hpp"

#include <cmath>
#include <queue>

namespace {
// 8 hướng lân cận: 4 hướng thẳng rồi 4 hướng chéo
const int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
const int kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
}  // namespace

void PathField::build(const GameMap& map, sf::Vector2f target) {
    m_width = map.getNavWidth();
    m_height = map.getNavHeight();
    m_cellSize = map.getNavCellSize();
    m_dist.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), -1);

    // 1) Ô xuất phát = ô chứa mục tiêu. Nếu ô đó bị "phình" vật cản chiếm (Player đứng sát cây), tìm ô trống gần nhất.
    int sx = static_cast<int>(std::floor(target.x / m_cellSize));
    int sy = static_cast<int>(std::floor(target.y / m_cellSize));
    if (map.isNavBlocked(sx, sy)) {
        bool found = false;
        for (int ring = 1; ring <= 6 && !found; ++ring) {
            for (int dy = -ring; dy <= ring && !found; ++dy) {
                for (int dx = -ring; dx <= ring && !found; ++dx) {
                    if (!map.isNavBlocked(sx + dx, sy + dy)) {
                        sx += dx;
                        sy += dy;
                        found = true;
                    }
                }
            }
        }
        if (!found) return;  // mục tiêu bị nhốt hoàn toàn: không có trường đường đi
    }

    // 2) BFS: hàng đợi (queue) vào trước ra trước => các ô được gán số bước theo thứ tự tăng dần.
    std::queue<int> frontier;
    m_dist[static_cast<std::size_t>(sy) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(sx)] = 0;
    frontier.push(sy * m_width + sx);
    while (!frontier.empty()) {
        const int current = frontier.front();
        frontier.pop();
        const int cx = current % m_width;
        const int cy = current / m_width;
        for (int i = 0; i < 8; ++i) {
            const int nx = cx + kDx[i];
            const int ny = cy + kDy[i];
            if (map.isNavBlocked(nx, ny)) continue;
            if (m_dist[static_cast<std::size_t>(ny) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(nx)] != -1) continue;
            // đi chéo thì hai ô thẳng kề bên cũng phải trống (không cho "cắt góc" xuyên qua góc vật cản)
            if (i >= 4 && (map.isNavBlocked(cx + kDx[i], cy) || map.isNavBlocked(cx, cy + kDy[i]))) continue;
            m_dist[static_cast<std::size_t>(ny) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(nx)] =
                m_dist[static_cast<std::size_t>(cy) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(cx)] + 1;
            frontier.push(ny * m_width + nx);
        }
    }
}

sf::Vector2f PathField::getDirection(const GameMap& map, sf::Vector2f from, float radius) const {
    if (m_dist.empty()) return {0.f, 0.f};
    auto distAt = [this](int x, int y) {
        if (x < 0 || y < 0 || x >= m_width || y >= m_height) return -1;
        return m_dist[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x)];
    };
    auto cellCenter = [this](int x, int y) {
        return sf::Vector2f((static_cast<float>(x) + 0.5f) * m_cellSize, (static_cast<float>(y) + 0.5f) * m_cellSize);
    };

    const int x = static_cast<int>(std::floor(from.x / m_cellSize));
    const int y = static_cast<int>(std::floor(from.y / m_cellSize));

    // Đang đứng trong "vùng đệm" sát vật cản (ô không có số): đi tới ô có số gần đó, ưu tiên ô có số bước NHỎ
    // (tức là tiến gần mục tiêu) thay vì ô gần nhất - nếu không sẽ bị kéo ngược lại và dao động qua lại.
    if (distAt(x, y) < 0) {
        float bestScore = 1.0e9f;
        sf::Vector2f bestTarget = from;
        bool found = false;
        for (int dy = -3; dy <= 3; ++dy) {
            for (int dx = -3; dx <= 3; ++dx) {
                const int d = distAt(x + dx, y + dy);
                if (d < 0) continue;
                const float score = static_cast<float>(d) + 0.7f * std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (score < bestScore) {
                    bestScore = score;
                    bestTarget = cellCenter(x + dx, y + dy);
                    found = true;
                }
            }
        }
        return found ? util::normalized(bestTarget - from) : sf::Vector2f(0.f, 0.f);
    }

    // Đi theo "dốc xuống": lần lượt chọn ô lân cận có số bước nhỏ nhất (tối đa 4 bước), ghi lại tâm các ô đó.
    sf::Vector2f waypoints[4];
    int count = 0;
    int cx = x, cy = y;
    for (int step = 0; step < 4; ++step) {
        const int here = distAt(cx, cy);
        if (here <= 0) break;  // đã tới ô mục tiêu
        int bestI = -1;
        int bestDist = here;
        for (int i = 0; i < 8; ++i) {
            const int d = distAt(cx + kDx[i], cy + kDy[i]);
            if (d < 0 || d >= bestDist) continue;
            if (i >= 4 && (map.isNavBlocked(cx + kDx[i], cy) || map.isNavBlocked(cx, cy + kDy[i]))) continue;
            bestDist = d;
            bestI = i;
        }
        if (bestI < 0) break;
        cx += kDx[bestI];
        cy += kDy[bestI];
        waypoints[count++] = cellCenter(cx, cy);
    }
    if (count == 0) return util::normalized(cellCenter(x, y) - from);

    // Chọn điểm đích tạm thời XA NHẤT mà đường thẳng từ 'from' tới đó vẫn thông thoáng (đi mượt, không cắt góc vật cản).
    // Không có điểm nào thông thoáng thì đi từng ô một (ô kế tiếp chắc chắn đủ chỗ vì lưới đã được "phình" vật cản).
    sf::Vector2f target = waypoints[0];
    for (int i = count - 1; i >= 0; --i) {
        if (map.isLineFree(from, waypoints[i], radius * 1.4f)) {  // 1.4: dư một chút so với bán kính thật để không cắt góc
            target = waypoints[i];
            break;
        }
    }
    return util::normalized(target - from);
}

int PathField::getDistance(sf::Vector2f from) const {
    if (m_dist.empty()) return -1;
    const int x = static_cast<int>(std::floor(from.x / m_cellSize));
    const int y = static_cast<int>(std::floor(from.y / m_cellSize));
    int best = -1;
    for (int dy = -3; dy <= 3; ++dy) {  // đứng sát vật cản (ô không có số) thì lấy ô gần nhất có số
        for (int dx = -3; dx <= 3; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= m_width || ny >= m_height) continue;
            const int d = m_dist[static_cast<std::size_t>(ny) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(nx)];
            if (d >= 0 && (best < 0 || d < best)) best = d;
        }
    }
    return best;
}
