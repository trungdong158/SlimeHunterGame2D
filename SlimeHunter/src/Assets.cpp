#include "Assets.hpp"

#include <algorithm>
#include <iostream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

void setPixelSafe(sf::Image& img, int x, int y, sf::Color c) {
    if (x >= 0 && x < static_cast<int>(img.getSize().x) &&
        y >= 0 && y < static_cast<int>(img.getSize().y)) {
        img.setPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)}, c);
    }
}

void fillBox(sf::Image& img, int x0, int y0, int w, int h, sf::Color c) {
    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) {
            setPixelSafe(img, x, y, c);
        }
    }
}

void ensureMonsterAssets(const fs::path& assetDir) {
    std::vector<fs::path> dirs = {assetDir};
#ifdef SLIMEHUNTER_SOURCE_ASSET_DIR
    dirs.emplace_back(SLIMEHUNTER_SOURCE_ASSET_DIR);
#endif

    for (const auto& base : dirs) {
        const fs::path monsterDir = base / "images" / "monsters";
        std::error_code ec;
        fs::create_directories(monsterDir, ec);

        // 1) Skeleton sprite sheet: 192x240 (4 frames x 5 rows, ô 48x48)
        const fs::path skelPath = monsterDir / "skeleton.png";
        if (!fs::exists(skelPath, ec)) {
            sf::Image img({192u, 240u}, sf::Color::Transparent);
            const sf::Color bone(235, 235, 230);
            const sf::Color boneDark(160, 160, 170);
            const sf::Color eyeRed(255, 45, 45);
            const sf::Color wood(150, 95, 45);

            for (int row = 0; row < 5; ++row) {
                const int frameCount = (row == 3) ? 2 : 4;
                for (int col = 0; col < frameCount; ++col) {
                    const int cx = col * 48 + 24;
                    const int cy = row * 48;

                    if (row == 4) {
                        // Death: các mẩu xương rơi rụng xuống đất
                        const int dropY = cy + 28 + col * 4;
                        fillBox(img, cx - 7, dropY, 14, 5, bone);
                        fillBox(img, cx - 5, dropY - 4, 10, 4, boneDark);
                        fillBox(img, cx - 2, dropY - 2, 2, 2, eyeRed);
                    } else {
                        const int bob = (row == 1 && (col % 2 == 1)) ? -2 : 0;
                        const int headY = cy + 12 + bob;

                        // Hộp sọ
                        fillBox(img, cx - 6, headY, 12, 10, bone);
                        fillBox(img, cx - 4, headY + 10, 8, 3, boneDark);
                        setPixelSafe(img, cx - 3, headY + 4, eyeRed);
                        setPixelSafe(img, cx + 2, headY + 4, eyeRed);

                        // Xương sống & xương sườn
                        const int spineY = headY + 13;
                        fillBox(img, cx - 1, spineY, 2, 9, boneDark);
                        fillBox(img, cx - 5, spineY + 2, 10, 2, bone);
                        fillBox(img, cx - 4, spineY + 5, 8, 2, bone);

                        // Chân
                        const int legY = spineY + 9;
                        if (row == 1) {
                            const int legOff = (col % 2 == 0) ? 2 : -2;
                            fillBox(img, cx - 4 + legOff, legY, 2, 10, bone);
                            fillBox(img, cx + 2 - legOff, legY, 2, 10, bone);
                        } else {
                            fillBox(img, cx - 4, legY, 2, 10, bone);
                            fillBox(img, cx + 2, legY, 2, 10, bone);
                        }

                        // Cung tên ở tay
                        if (row == 2) {
                            fillBox(img, cx + 5, spineY - 2, 2, 14, wood);
                            fillBox(img, cx + 7, spineY + 2, 2, 6, wood);
                            fillBox(img, cx - 2, spineY + 4, 10, 1, sf::Color(240, 240, 240));
                        } else {
                            fillBox(img, cx + 5, spineY + 1, 2, 10, wood);
                        }
                    }
                }
            }
            (void)img.saveToFile(skelPath);
        }

        // 2) Bat sprite sheet: 192x160 (4 frames x 4 rows, ô 48x40)
        const fs::path batPath = monsterDir / "bat.png";
        if (!fs::exists(batPath, ec)) {
            sf::Image img({192u, 160u}, sf::Color::Transparent);
            const sf::Color body(65, 45, 85);
            const sf::Color wing(95, 55, 125);
            const sf::Color eyeYellow(255, 220, 60);

            for (int row = 0; row < 4; ++row) {
                const int frameCount = (row == 2) ? 2 : 4;
                for (int col = 0; col < frameCount; ++col) {
                    const int cx = col * 48 + 24;
                    const int cy = row * 40;

                    if (row == 3) {
                        const int dropY = cy + 18 + col * 4;
                        fillBox(img, cx - 5, dropY, 10, 8, body);
                        fillBox(img, cx - 9, dropY + 2, 18, 3, wing);
                    } else {
                        const int bodyY = cy + 16;
                        fillBox(img, cx - 4, bodyY, 8, 10, body);
                        setPixelSafe(img, cx - 4, bodyY - 2, body);
                        setPixelSafe(img, cx - 3, bodyY - 1, body);
                        setPixelSafe(img, cx + 3, bodyY - 2, body);
                        setPixelSafe(img, cx + 2, bodyY - 1, body);
                        setPixelSafe(img, cx - 2, bodyY + 3, eyeYellow);
                        setPixelSafe(img, cx + 1, bodyY + 3, eyeYellow);
                        setPixelSafe(img, cx - 2, bodyY + 7, sf::Color::White);
                        setPixelSafe(img, cx + 1, bodyY + 7, sf::Color::White);

                        if (col == 0) {
                            fillBox(img, cx - 14, bodyY - 6, 9, 7, wing);
                            fillBox(img, cx + 5, bodyY - 6, 9, 7, wing);
                        } else if (col == 1) {
                            fillBox(img, cx - 16, bodyY, 11, 6, wing);
                            fillBox(img, cx + 5, bodyY, 11, 6, wing);
                        } else if (col == 2) {
                            fillBox(img, cx - 14, bodyY + 4, 9, 7, wing);
                            fillBox(img, cx + 5, bodyY + 4, 9, 7, wing);
                        } else {
                            fillBox(img, cx - 12, bodyY, 8, 6, wing);
                            fillBox(img, cx + 4, bodyY, 8, 6, wing);
                        }
                    }
                }
            }
            (void)img.saveToFile(batPath);
        }
    }
}

}  // namespace

Assets::Assets() {
    m_assetDir = findAssetDirectory();
    std::cout << "[Assets] Thu muc tai nguyen: " << m_assetDir.string() << '\n';

    // Đảm bảo các asset quái vật mới tự động được tạo nếu chưa có
    ensureMonsterAssets(m_assetDir);

    loadFont();

    loadLevels();

    loadTexture(playerSheet, "images/player/player_sheet.png", {240u, 200u}, sf::Color(255, 0, 255));
    loadTexture(playerBat, "images/player/player_bat.png", {21u, 6u}, sf::Color(200, 140, 70));
    loadTexture(shadow, "images/effects/shadow.png", {26u, 10u}, sf::Color(0, 0, 0, 90));

    loadTexture(slimeGreen, "images/monsters/slime_green.png", {288u, 200u}, sf::Color(60, 255, 60));
    loadTexture(slimePurple, "images/monsters/slime_purple.png", {288u, 200u}, sf::Color(170, 80, 255));
    loadTexture(slimeBlue, "images/monsters/slime_blue.png", {288u, 200u}, sf::Color(60, 110, 255));
    loadTexture(slimeKing, "images/monsters/slime_king.png", {480u, 320u}, sf::Color(255, 80, 40));

    loadTexture(skeletonSheet, "images/monsters/skeleton.png", {192u, 240u}, sf::Color(220, 220, 230));
    loadTexture(batSheet, "images/monsters/bat.png", {192u, 160u}, sf::Color(90, 40, 130));

    for (const std::string& warning : m_warnings) std::cerr << "[Assets] CANH BAO: " << warning << '\n';
}

fs::path Assets::findAssetDirectory() const {
    std::vector<fs::path> candidates = {"assets", "../assets", "../../assets"};
#ifdef SLIMEHUNTER_SOURCE_ASSET_DIR
    candidates.emplace_back(SLIMEHUNTER_SOURCE_ASSET_DIR);
#endif
    for (const fs::path& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate / "images", ec)) return candidate;
    }
    return "assets";
}

bool Assets::loadTexture(sf::Texture& texture, const std::string& relativePath, sf::Vector2u fallbackSize,
                         sf::Color fallbackColor) {
    const fs::path fullPath = m_assetDir / relativePath;
    std::error_code ec;
    if (fs::exists(fullPath, ec) && texture.loadFromFile(fullPath)) {
        return true;
    }

    m_warnings.push_back("Khong tim thay anh '" + relativePath + "' - dung hinh thay the.");
    const sf::Image placeholder(fallbackSize, fallbackColor);
    if (!texture.loadFromImage(placeholder)) {
        m_warnings.push_back("Khong tao duoc hinh thay the cho '" + relativePath + "'.");
    }
    return false;
}

void Assets::loadFont() {
    const fs::path bundled = m_assetDir / "fonts" / "DejaVuSans-Bold.ttf";
    if (font.openFromFile(bundled)) {
        hasFont = true;
        return;
    }
    if (font.openFromFile("C:/Windows/Fonts/arialbd.ttf") || font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        hasFont = true;
        m_warnings.push_back("Khong tim thay assets/fonts/DejaVuSans-Bold.ttf - dang dung font Arial cua he thong.");
        return;
    }
    m_warnings.push_back("Khong tim thay font chu. Hay chep DejaVuSans-Bold.ttf vao assets/fonts/.");
}

void Assets::loadLevels() {
    for (int id = 1; id <= kLevelCount; ++id) {
        const LevelConfig& config = getLevelConfig(id);
        const std::size_t index = static_cast<std::size_t>(id - 1);
        loadTexture(levelBackground[index], config.background, {1800u, 1200u}, sf::Color(60, 90, 60));
        if (!levelCollision[index].loadFromFile(m_assetDir / config.collision)) {
            m_warnings.push_back(std::string("Khong tim thay '") + config.collision + "' - level " + std::to_string(id) + " se khong co vat can.");
            levelCollision[index] = sf::Image(sf::Vector2u(600u, 400u), sf::Color::Black);
        }
    }
}

const sf::Texture& Assets::getLevelBackground(int levelId) const {
    return levelBackground[static_cast<std::size_t>(std::min(std::max(levelId, 1), kLevelCount) - 1)];
}

const sf::Image& Assets::getLevelCollision(int levelId) const {
    return levelCollision[static_cast<std::size_t>(std::min(std::max(levelId, 1), kLevelCount) - 1)];
}
