#include "Game.hpp"

#include "Utils.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <imm.h>

static void disableImeForWindow(sf::WindowHandle handle) {
    if (!handle) return;
    HWND hwnd = static_cast<HWND>(handle);
    ImmAssociateContext(hwnd, NULL);
}
#endif

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
const float kWindowW = 1280.f;
const float kWindowH = 720.f;
const float kPi2 = 6.2831853f;
const float kLevelCompleteMinTime = 2.6f;
const float kFadeTime = 0.4f;
const std::size_t kMaxFieldLength = 32;
}  // namespace

// ============================================================================
//  Khởi tạo và trạng thái
// ============================================================================
Game::Game(const std::string& dataDirOverride)
    : m_window(sf::VideoMode({1280u, 720u}), "SLIME HUNTER", sf::Style::Default, sf::State::Windowed),
      m_assets(),
      m_map(m_assets, 1),
      m_player(m_assets),
      m_repo(dataDirOverride.empty() ? UserRepository::findDataDirectory() : std::filesystem::path(dataDirOverride)),
      m_audio(),
      m_dispatcher(),
      m_camera(sf::FloatRect({0.f, 0.f}, {kWindowW, kWindowH})),
      m_uiView(sf::FloatRect({0.f, 0.f}, {kWindowW, kWindowH})) {
    m_window.setVerticalSyncEnabled(true);
#ifdef _WIN32
    disableImeForWindow(m_window.getNativeHandle());
#endif
    m_progress = UserRepository::defaultProgress(0);

    m_dispatcher.addObserver(&m_audio);

    std::cout << "[Data] Thu muc du lieu: " << m_repo.getDataDir().string() << (m_repo.isUsable() ? "" : "  (KHONG ghi duoc!)") << '\n';
    if (!m_repo.isUsable()) {
        m_loginMessage = "Không ghi được thư mục data/ - hãy nhấn F3 để chơi khách (không lưu).";
        m_loginMessageIsError = true;
        std::cerr << "[Data] CANH BAO: " << m_repo.getLastError() << '\n';
    }
    previewLevel(1);
    setState(GameState::Login);
}

void Game::setState(GameState state) {
    m_state = state;
    m_window.setKeyRepeatEnabled(state == GameState::Login);
}

void Game::toggleFullscreen() {
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(m_window.getNativeHandle());
    if (hwnd) {
        if (IsZoomed(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
            showToast("Chế độ: Cửa sổ", 1.5f);
        } else {
            ShowWindow(hwnd, SW_MAXIMIZE);
            showToast("Chế độ: Toàn màn hình (F11)", 1.5f);
        }
    }
#endif
}

void Game::showToast(const std::string& text, float seconds) {
    m_toast = text;
    m_toastTimer = seconds;
}

// ============================================================================
//  GAME LOOP
// ============================================================================
void Game::run() {
    sf::Clock clock;
    while (m_window.isOpen()) {
        float dt = clock.restart().asSeconds();
        dt = std::min(dt, 0.05f);
        processEvents();
        update(dt);
        render();
    }
}

void Game::processEvents() {
    while (const std::optional event = m_window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            saveOnExit();
            m_window.close();
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            handleKeyPressed(key->code);
        } else if (const auto* text = event->getIf<sf::Event::TextEntered>()) {
            handleTextEntered(text->unicode);
        } else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
            if (m_state == GameState::Playing) {
                cycleWeapon(wheel->delta > 0.f ? 1 : -1);
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouse->button == sf::Mouse::Button::Left) {
                const sf::Vector2f mpos = m_window.mapPixelToCoords(mouse->position, m_uiView);
                if (m_state == GameState::Login) {
                    handleLoginClick(mpos);
                } else if (m_state == GameState::Menu) {
                    handleMenuClick(mpos);
                } else if (m_state == GameState::PowerUpSelect) {
                    // Click chuột chọn thẻ Rogue-lite với hitbox dùng chung đồng bộ 100%
                    const auto layout = getPowerUpCardLayout(m_uiView.getSize());
                    for (int i = 0; i < 3; ++i) {
                        if (layout.contains(i, mpos)) {
                            selectPowerUp(i);
                            break;
                        }
                    }
                } else if (m_state == GameState::Shop) {
                    handleShopClick(mpos);
                } else if (m_state == GameState::Leaderboard) {
                    setState((m_leaderboardPrevState == GameState::Playing || m_leaderboardPrevState == GameState::Paused)
                                 ? m_leaderboardPrevState : GameState::Menu);
                } else if (m_state == GameState::GameOver) {
                    startRun(m_levelId);
                } else if (m_state == GameState::Victory) {
                    goToMenu();
                } else if (m_state == GameState::LevelComplete) {
                    if (m_levelCompleteTimer > 0.8f) {
                        m_levelCompleteTimer = std::max(m_levelCompleteTimer, kLevelCompleteMinTime);
                    }
                }
            }
        } else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            const sf::Vector2f size(resized->size);
            if (size.x > 0.f && size.y > 0.f) {
                m_camera.setSize(size);
                m_uiView = sf::View(sf::FloatRect({0.f, 0.f}, size));
            }
        } else if (event->is<sf::Event::FocusGained>()) {
#ifdef _WIN32
            disableImeForWindow(m_window.getNativeHandle());
#endif
        } else if (event->is<sf::Event::FocusLost>()) {
            if (m_state == GameState::Playing && !m_autopilot) setState(GameState::Paused);
        }
    }
}

void Game::handleLoginClick(const sf::Vector2f& mpos) {
    const float w = m_uiView.getSize().x;
    const float boxW = 480.f;
    const float boxX = (w - boxW) / 2.f;

    // Nút Con Mắt (Ẩn / Hiện mật khẩu) nằm ở mép phải ô Mật khẩu
    const sf::FloatRect eyeBounds({boxX + boxW - 40.f, 333.f + 6.f}, {34.f, 30.f});
    if (eyeBounds.contains(mpos)) {
        m_showPassword = !m_showPassword;
        m_loginField = 1; // Giữ con trỏ nhập liệu ở ô Mật khẩu
        m_audio.playPickup();
        return;
    }

    // Click vào ô Tên đăng nhập
    if (mpos.x >= boxX && mpos.x <= boxX + boxW && mpos.y >= 220.f && mpos.y <= 298.f) {
        m_loginField = 0;
        return;
    }
    // Click vào ô Mật khẩu
    if (mpos.x >= boxX && mpos.x <= boxX + boxW && mpos.y >= 300.f && mpos.y <= 378.f) {
        m_loginField = 1;
        return;
    }
    // Click vào nút Đăng nhập / Tiếp tục / Tạo tài khoản
    if (mpos.y >= 405.f && mpos.y <= 445.f && std::abs(mpos.x - w / 2.f) < 220.f) {
        if (m_loginField == 0 && !m_loginUser.empty()) {
            m_loginField = 1;
        } else {
            submitLoginForm();
        }
        return;
    }
    // Click vào nút F2 đổi chế độ / F3 Chơi khách
    if (mpos.y >= 450.f && mpos.y <= 485.f) {
        if (mpos.x < w / 2.f) {
            m_registerMode = !m_registerMode;
            m_loginMessage.clear();
        } else {
            startGuestSession();
        }
        return;
    }
}

void Game::cycleWeapon(int direction) {
    WeaponType current = WeaponType::BaseballBat;
    if (m_player.getWeapon()) current = m_player.getWeapon()->getType();
    WeaponType next = WeaponType::BaseballBat;
    if (current == WeaponType::BaseballBat) {
        next = (direction > 0) ? WeaponType::MagicWand : WeaponType::Boomerang;
    } else if (current == WeaponType::MagicWand) {
        next = (direction > 0) ? WeaponType::Boomerang : WeaponType::BaseballBat;
    } else {
        next = (direction > 0) ? WeaponType::BaseballBat : WeaponType::MagicWand;
    }

    if (next == WeaponType::BaseballBat) {
        m_player.setWeapon(std::make_unique<BaseballBat>());
        showToast("Vũ khí: Gậy Bóng Chày (Cận chiến)", 1.8f);
        m_audio.playSlash();
    } else if (next == WeaponType::MagicWand) {
        m_player.setWeapon(std::make_unique<MagicWand>());
        showToast("Vũ khí: Gậy Phép Thuật (Cầu phép tầm xa)", 1.8f);
        m_audio.playPickup();
    } else {
        m_player.setWeapon(std::make_unique<BoomerangWeapon>());
        showToast("Vũ khí: Boomerang (Ném 2 chiều)", 1.8f);
        m_audio.playSlash();
    }
}

void Game::handleMenuClick(const sf::Vector2f& mpos) {
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;
    const int maxSelectable = std::min(m_progress.maxLevelUnlocked, kLevelCount);

    // Nút Bảng Xếp Hạng [B]
    const float btnY = h * 0.69f;
    if (std::abs(mpos.x - w * 0.32f) < 170.f && std::abs(mpos.y - btnY) < 28.f) {
        openLeaderboard();
        return;
    }
    // Nút Cửa Hàng [S]
    if (std::abs(mpos.x - w * 0.68f) < 170.f && std::abs(mpos.y - btnY) < 28.f) {
        openShop();
        return;
    }
    // Mũi tên chuyển màn <
    if (std::abs(mpos.x - (w / 2.f - 240.f)) < 40.f && std::abs(mpos.y - h * 0.29f) < 30.f) {
        m_selectedLevel = std::max(1, m_selectedLevel - 1);
        previewLevel(m_selectedLevel);
        return;
    }
    // Mũi tên chuyển màn >
    if (std::abs(mpos.x - (w / 2.f + 240.f)) < 40.f && std::abs(mpos.y - h * 0.29f) < 30.f) {
        m_selectedLevel = std::min(maxSelectable, m_selectedLevel + 1);
        previewLevel(m_selectedLevel);
        return;
    }
    // Bấm vào bất kỳ đâu khác trên menu để Bắt đầu chơi
    startRun(m_selectedLevel);
}

void Game::handleShopClick(const sf::Vector2f& mpos) {
    const float w = m_uiView.getSize().x;
    const float h = m_uiView.getSize().y;

    // Nút quay lại Menu ở chân màn hình
    if (mpos.y >= h - 75.f && mpos.y <= h - 35.f) {
        setState(GameState::Menu);
        return;
    }

    // Các thẻ nâng cấp
    float cardY = 190.f;
    const float boxW = 750.f;
    const float boxH = 75.f;
    const float boxX = (w - boxW) / 2.f;

    for (int i = 0; i < static_cast<int>(m_shopItems.size()); ++i) {
        sf::FloatRect itemBounds({boxX, cardY}, {boxW, boxH});
        if (itemBounds.contains(mpos)) {
            m_selectedShopIndex = i;
            if (ShopSystem::buyItem(m_shopItems[i].id, m_progress, m_repo)) {
                showToast("Nâng cấp thành công!", 2.5f);
                m_audio.playLevelUp();
                m_shopItems = ShopSystem::getShopItems(m_progress);
            } else if (!m_shopItems[i].purchased) {
                showToast("Không đủ điểm tích lũy!", 2.5f);
            }
            return;
        }
        cardY += 95.f;
    }
}

void Game::handleTextEntered(char32_t unicode) {
    if (m_state != GameState::Login) return;
    std::string& field = (m_loginField == 0) ? m_loginUser : m_loginPass;
    if (unicode == 8) {
        if (!field.empty()) field.pop_back();
    } else if (unicode >= 33 && unicode <= 126 && field.size() < kMaxFieldLength) {
        field.push_back(static_cast<char>(unicode));
    }
}

void Game::handleKeyPressed(sf::Keyboard::Key key) {
    using Key = sf::Keyboard::Key;

    // F1 hoặc H: Bật/Tắt xem hộp va chạm debug (tiện lợi cho cả bàn phím laptop cần Fn)
    // F1 hoạt động ở mọi màn hình; H hoạt động ở mọi màn hình trừ ô nhập chữ Login
    if (key == Key::F1 || (key == Key::H && m_state != GameState::Login)) {
        m_debug = !m_debug;
        showToast(m_debug ? "Debug hộp va chạm: BẬT (F1/H)" : "Debug hộp va chạm: TẮT", 2.f);
        return;
    }

    // F11 hoặc Alt+Enter: Bật/Tắt chế độ toàn màn hình
    if (key == Key::F11 || (key == Key::Enter && (sf::Keyboard::isKeyPressed(Key::LAlt) || sf::Keyboard::isKeyPressed(Key::RAlt)))) {
        toggleFullscreen();
        return;
    }

    // M: Bật/Tắt âm thanh toàn cục (ngoại trừ lúc đang gõ tên/pass ở màn hình Login)
    if (key == Key::M && m_state != GameState::Login) {
        m_audio.toggleMute();
        showToast(m_audio.isMuted() ? "Âm thanh: TẮT" : "Âm thanh: BẬT", 2.f);
        return;
    }

    switch (m_state) {
        case GameState::Login:
            if (key == Key::Enter) {
                if (m_loginField == 0) {
                    m_loginField = 1; // Nhập tên xong bấm Enter -> tự động chuyển sang ô mật khẩu
                } else {
                    submitLoginForm(); // Ở ô mật khẩu bấm Enter -> đăng nhập
                }
            }
            else if (key == Key::Up) m_loginField = 0;
            else if (key == Key::Down) m_loginField = 1;
            else if (key == Key::Tab) m_loginField = 1 - m_loginField;
            else if (key == Key::F2) {
                m_registerMode = !m_registerMode;
                m_loginMessage.clear();
            } else if (key == Key::F3) startGuestSession();
            else if (key == Key::Escape) m_window.close();
            break;

        case GameState::Menu: {
            const int maxSelectable = std::min(m_progress.maxLevelUnlocked, kLevelCount);
            if (key == Key::Enter || key == Key::Space) startRun(m_selectedLevel);
            else if (key == Key::Left || key == Key::A) {
                m_selectedLevel = std::max(1, m_selectedLevel - 1);
                previewLevel(m_selectedLevel);
            } else if (key == Key::Right || key == Key::D) {
                m_selectedLevel = std::min(maxSelectable, m_selectedLevel + 1);
                previewLevel(m_selectedLevel);
            } else if (key == Key::B) {
                openLeaderboard();
            } else if (key == Key::S) {
                openShop();
            } else if (key == Key::L) logout();
            else if (key == Key::Escape) {
                saveOnExit();
                m_window.close();
            }
            break;
        }

        case GameState::Playing:
            if (key == Key::Escape) setState(GameState::Paused);
            else if (key == Key::B) openLeaderboard();
            else if (key == Key::Num1 || key == Key::Numpad1) {
                m_player.setWeapon(std::make_unique<BaseballBat>());
                showToast("Vũ khí: Gậy Bóng Chày (Cận chiến)", 2.f);
                m_audio.playSlash();
            } else if (key == Key::Num2 || key == Key::Numpad2) {
                m_player.setWeapon(std::make_unique<MagicWand>());
                showToast("Vũ khí: Gậy Phép Thuật (Cầu phép tầm xa)", 2.f);
                m_audio.playPickup();
            } else if (key == Key::Num3 || key == Key::Numpad3) {
                m_player.setWeapon(std::make_unique<BoomerangWeapon>());
                showToast("Vũ khí: Boomerang (Ném 2 chiều)", 2.f);
                m_audio.playSlash();
            } else if (key == Key::Q) {
                cycleWeapon(-1);
            } else if (key == Key::E) {
                cycleWeapon(1);
            } else if (key == Key::Space) {
                if (m_player.tryDash()) {
                    m_dispatcher.dispatch(GameEvent::PlayerDashed);
                }
            }
            break;

        case GameState::Paused:
            if (key == Key::Escape || key == Key::Enter) setState(GameState::Playing);
            else if (key == Key::B) openLeaderboard();
            else if (key == Key::Q) goToMenu();
            break;

        case GameState::LevelComplete:
            if ((key == Key::Enter || key == Key::Space) && m_levelCompleteTimer > 0.8f)
                m_levelCompleteTimer = std::max(m_levelCompleteTimer, kLevelCompleteMinTime);
            break;

        case GameState::GameOver:
            if (key == Key::R || key == Key::Enter) startRun(m_levelId);
            else if (key == Key::Escape) goToMenu();
            break;

        case GameState::Victory:
            if (key == Key::Enter || key == Key::Escape) goToMenu();
            break;

        case GameState::Leaderboard:
            if (key == Key::Escape || key == Key::Enter || key == Key::B || key == Key::Backspace || key == Key::Space) {
                setState((m_leaderboardPrevState == GameState::Playing || m_leaderboardPrevState == GameState::Paused)
                             ? m_leaderboardPrevState : GameState::Menu);
            }
            break;

        case GameState::Shop:
            if (key == Key::Escape || key == Key::B || key == Key::Backspace) setState(GameState::Menu);
            else if (key == Key::Up || key == Key::W) m_selectedShopIndex = std::max(0, m_selectedShopIndex - 1);
            else if (key == Key::Down || key == Key::S) m_selectedShopIndex = std::min(static_cast<int>(m_shopItems.size()) - 1, m_selectedShopIndex + 1);
            else if (key == Key::Enter || key == Key::Space) {
                if (!m_shopItems.empty()) {
                    if (ShopSystem::buyItem(m_shopItems[m_selectedShopIndex].id, m_progress, m_repo)) {
                        showToast("Nâng cấp thành công!", 2.5f);
                        m_audio.playLevelUp();
                        m_shopItems = ShopSystem::getShopItems(m_progress);
                    } else if (!m_shopItems[m_selectedShopIndex].purchased) {
                        showToast("Không đủ điểm tích lũy!", 2.5f);
                    }
                }
            }
            break;

        case GameState::PowerUpSelect:
            if (key == Key::Num1 || key == Key::Numpad1) selectPowerUp(0);
            else if (key == Key::Num2 || key == Key::Numpad2) selectPowerUp(1);
            else if (key == Key::Num3 || key == Key::Numpad3) selectPowerUp(2);
            break;
    }
}

void Game::update(float dt) {
    m_fps = util::lerp(m_fps, dt > 0.f ? 1.f / dt : 60.f, 0.1f);
    m_menuTime += dt;
    m_toastTimer = std::max(0.f, m_toastTimer - dt);
    updateFade(dt);

    switch (m_state) {
        case GameState::Login:
        case GameState::Menu:
        case GameState::Leaderboard:
        case GameState::Shop:
            m_player.update(dt, PlayerInput{}, m_map);
            m_effects.updateAmbient(dt);
            break;
        case GameState::Playing:
            updatePlaying(dt);
            break;
        case GameState::PowerUpSelect:
            m_effects.updateAmbient(dt);
            break;
        case GameState::LevelComplete:
            updateLevelComplete(dt);
            break;
        case GameState::Paused:
        case GameState::GameOver:
        case GameState::Victory:
            break;
    }
}

// ============================================================================
//  TÀI KHOẢN VÀ TIẾN TRÌNH
// ============================================================================
void Game::submitLoginForm() {
    m_loginMessageIsError = true;
    if (!m_repo.isUsable()) {
        m_loginMessage = "Không dùng được thư mục data/. Hãy nhấn F3 để chơi khách.";
        return;
    }

    UserRecord user;
    if (m_registerMode) {
        switch (m_repo.registerUser(m_loginUser, m_loginPass, &user)) {
            case AuthResult::Ok:
                break;
            case AuthResult::InvalidUsername:
                m_loginMessage = "Tên đăng nhập: 3-20 ký tự gồm chữ không dấu, số, _ hoặc -";
                return;
            case AuthResult::InvalidPassword:
                m_loginMessage = "Mật khẩu: 4-32 ký tự, không có khoảng trắng, dấu phẩy hoặc ngoặc kép";
                return;
            case AuthResult::UsernameTaken:
                m_loginMessage = "Tên đăng nhập đã tồn tại, hãy chọn tên khác";
                return;
            case AuthResult::WrongCredentials:
            case AuthResult::StorageError:
                m_loginMessage = "Không ghi được dữ liệu tài khoản";
                return;
        }
    } else {
        if (m_repo.login(m_loginUser, m_loginPass, user) != AuthResult::Ok) {
            m_loginMessage = "Sai tên đăng nhập hoặc mật khẩu";
            return;
        }
    }

    m_user = user;
    m_guest = false;
    m_progress = m_repo.loadProgress(user.userId);
    for (const std::string& warning : m_repo.getWarnings()) std::cerr << "[Data] " << warning << '\n';
    const bool justRegistered = m_registerMode;
    m_loginPass.assign(m_loginPass.size(), '\0');
    m_loginPass.clear();
    m_showPassword = false;
    m_loginMessage.clear();
    m_registerMode = false;
    enterMenuWithProgress();
    if (justRegistered) showToast("Đã tạo tài khoản. Chúc bạn chơi vui!", 3.5f);
}

void Game::startGuestSession() {
    m_user.reset();
    m_guest = true;
    m_progress = UserRepository::defaultProgress(0);
    m_loginMessage.clear();
    enterMenuWithProgress();
    showToast("Chơi khách: tiến trình KHÔNG được lưu", 4.f);
}

void Game::logout() {
    saveOnExit();
    m_user.reset();
    m_guest = false;
    m_progress = UserRepository::defaultProgress(0);
    m_loginUser.clear();
    m_loginPass.clear();
    m_showPassword = false;
    m_loginField = 0;
    m_registerMode = false;
    m_loginMessage = "Đã đăng xuất";
    m_loginMessageIsError = false;
    previewLevel(1);
    setState(GameState::Login);
}

void Game::enterMenuWithProgress() {
    m_progress.maxLevelUnlocked = std::min(std::max(m_progress.maxLevelUnlocked, 1), kLevelCount + 1);
    m_progress.currentLevel = std::min(std::max(m_progress.currentLevel, 1), std::min(m_progress.maxLevelUnlocked, kLevelCount));
    m_unsavedKills = 0;
    m_unsavedScore = 0;
    m_player.setBaseStats(m_progress.playerMaxHp, m_progress.playerDamage);
    m_player.setSlashTier(balance::slashTierForProgress(m_progress.maxLevelUnlocked));
    m_selectedLevel = m_progress.currentLevel;
    previewLevel(m_selectedLevel);
    setState(GameState::Menu);
}

bool Game::saveProgress() {
    if (!m_user.has_value()) return true;
    m_progress.userId = m_user->userId;
    const bool ok = m_repo.saveProgress(m_progress);
    m_lastSaveOk = ok;
    if (!ok) {
        std::cerr << "[Data] Khong luu duoc tien trinh: " << m_repo.getLastError() << '\n';
        showToast("Không lưu được tiến trình!", 4.f);
    }
    return ok;
}

void Game::flushTotals() {
    if (m_unsavedKills == 0 && m_unsavedScore == 0) return;
    const long long kills = static_cast<long long>(m_progress.totalKills) + m_unsavedKills;
    const long long score = static_cast<long long>(m_progress.totalScore) + m_unsavedScore;
    m_progress.totalKills = static_cast<int>(std::min(kills, 1000000000LL));
    m_progress.totalScore = static_cast<int>(std::min(score, 1000000000LL));
    m_unsavedKills = 0;
    m_unsavedScore = 0;
}

void Game::saveOnExit() {
    switch (m_state) {
        case GameState::Playing:
        case GameState::Paused:
        case GameState::LevelComplete:
        case GameState::GameOver:
        case GameState::Victory:
        case GameState::PowerUpSelect:
            flushTotals();
            saveProgress();
            break;
        case GameState::Login:
        case GameState::Menu:
        case GameState::Leaderboard:
        case GameState::Shop:
            break;
    }
}

// ============================================================================
//  LEVEL
// ============================================================================
void Game::previewLevel(int levelId) {
    m_levelId = std::min(std::max(levelId, 1), kLevelCount);
    const LevelConfig& config = getLevelConfig(m_levelId);
    m_map.loadLevel(m_assets, m_levelId);
    m_monsters.clear();
    m_projectiles.clear();
    m_items.clear();
    m_effects.clear();
    m_effects.setAmbient(config.ambient, config.ambientColor, m_map.getSize());
    m_player.teleportTo(m_map.findFreeSpotNear(config.spawn, 20.f));
    m_cameraPos = m_player.getPosition();
    updateCamera(0.f, true);
}

void Game::startRun(int levelId) {
    levelId = std::min(std::max(levelId, 1), std::min(m_progress.maxLevelUnlocked, kLevelCount));
    m_player.setBaseStats(m_progress.playerMaxHp, m_progress.playerDamage);
    m_player.setSlashTier(balance::slashTierForProgress(m_progress.maxLevelUnlocked));
    m_player.reset(getLevelConfig(levelId).spawn);

    m_score = 0;
    m_kills = 0;
    m_survivalTime = 0.f;
    m_gameOverDelay = 0.f;
    m_gameOverHandled = false;
    m_levelUpTimer = 0.f;
    m_levelCompleteTimer = 0.f;
    m_fadeStage = FadeStage::None;
    m_fadeAlpha = 0.f;

    loadLevel(levelId);
    m_progress.currentLevel = levelId;
    setState(GameState::Playing);
}

void Game::loadLevel(int levelId) {
    m_levelId = std::min(std::max(levelId, 1), kLevelCount);
    const LevelConfig& config = getLevelConfig(m_levelId);

    m_map.loadLevel(m_assets, m_levelId);
    m_monsters.clear();
    m_projectiles.clear();
    m_items.clear();
    m_effects.clear();
    m_effects.setAmbient(config.ambient, config.ambientColor, m_map.getSize());
    m_gate.setup(config.exit, config.exitRadius);

    m_phase = LevelPhase::Fighting;
    m_levelTotalToSpawn = config.totalMonsters + (config.bossAtEnd ? 1 : 0);
    m_spawnedCount = 0;
    m_levelKills = 0;
    m_spawnTimer = 1.2f;
    m_gateHintTimer = 0.f;
    m_rewardLines.clear();

    const sf::Vector2f spawn = m_map.findFreeSpotNear(config.spawn, 20.f);
    m_player.teleportTo(spawn);
    m_pathToPlayer.build(m_map, spawn);
    m_pathTimer = 0.f;
    m_autopilotPathTimer = 0.f;
    m_cameraPos = spawn;
    updateCamera(0.f, true);

    showToast("Màn " + std::to_string(m_levelId) + ": " + config.name, 3.f);
}

int Game::monstersRemaining() const { return (m_levelTotalToSpawn - m_spawnedCount) + countAliveMonsters(); }

void Game::updateLevelFlow(float dt) {
    m_gateHintTimer = std::max(0.f, m_gateHintTimer - dt);
    if (m_phase == LevelPhase::Completed) return;

    if (m_phase == LevelPhase::Fighting && m_spawnedCount >= m_levelTotalToSpawn && countAliveMonsters() == 0) {
        m_phase = LevelPhase::GateOpen;
        m_gate.setOpen(true);
        m_effects.addSparks(m_gate.getPosition(), sf::Color(160, 255, 255), 50);
        showToast("Cổng dịch chuyển đã mở! Hãy đi tới cổng sáng.", 5.f);
        m_dispatcher.dispatch(GameEvent::GateOpened);
    }

    const bool touching = m_gate.touches(m_player.getPosition(), m_player.getFootRadius());
    if (m_phase == LevelPhase::GateOpen && touching) {
        completeLevel();
    } else if (m_phase == LevelPhase::Fighting &&
               util::distance(m_player.getPosition(), m_gate.getPosition()) < m_gate.getRadius() + 200.f) {
        m_gateHintTimer = 0.3f;
    }
}

void Game::completeLevel() {
    if (m_phase == LevelPhase::Completed || m_player.isDead()) return;
    m_phase = LevelPhase::Completed;

    const int cleared = m_levelId;
    const bool firstClear = (cleared == m_progress.maxLevelUnlocked);
    const int oldTier = balance::slashTierForProgress(m_progress.maxLevelUnlocked);
    m_rewardLines.clear();

    if (firstClear) {
        const int oldHp = m_progress.playerMaxHp;
        const int oldDamage = m_progress.playerDamage;
        m_progress.playerMaxHp = balance::upgradedMaxHp(oldHp);
        m_progress.playerDamage = balance::upgradedDamage(oldDamage);
        m_progress.maxLevelUnlocked = std::min(cleared + 1, kLevelCount + 1);
        m_player.applyLevelUpgrade(m_progress.playerMaxHp, m_progress.playerDamage);
        m_rewardLines.push_back("Máu tối đa: " + std::to_string(oldHp) + " -> " + std::to_string(m_progress.playerMaxHp));
        m_rewardLines.push_back("Sát thương: " + std::to_string(oldDamage) + " -> " + std::to_string(m_progress.playerDamage));
        m_rewardLines.push_back("Đã hồi đầy máu");
        const int newTier = balance::slashTierForProgress(m_progress.maxLevelUnlocked);
        if (newTier > oldTier) m_rewardLines.push_back(std::string("MỞ KHÓA: ") + balance::slashProfileForTier(newTier).unlockText);
        if (cleared < kLevelCount) m_rewardLines.push_back("Mở khóa Màn " + std::to_string(cleared + 1) + ": " + getLevelConfig(cleared + 1).name);
    } else {
        m_player.heal(1.0e9f);
        m_rewardLines.push_back("Màn đã hoàn thành trước đó: không nhận thêm nâng cấp");
        m_rewardLines.push_back("Đã hồi đầy máu");
    }
    m_player.setSlashTier(balance::slashTierForProgress(m_progress.maxLevelUnlocked));
    m_progress.currentLevel = std::min(cleared + 1, std::min(m_progress.maxLevelUnlocked, kLevelCount));
    m_selectedLevel = m_progress.currentLevel;

    flushTotals();
    const bool saved = saveProgress();
    m_rewardLines.push_back(saved ? (m_user.has_value() ? "Tiến trình đã được lưu" : "Chơi khách: tiến trình không được lưu")
                                  : "KHÔNG lưu được tiến trình! (xem thông báo lỗi ở console)");

    m_dispatcher.dispatch(GameEvent::LevelCleared);
    m_levelCompleteTimer = 0.f;
    setState(cleared >= kLevelCount ? GameState::Victory : GameState::LevelComplete);
}

void Game::updateLevelComplete(float dt) {
    m_levelCompleteTimer += dt;
    m_effects.update(dt);
    m_effects.updateAmbient(dt);
    m_gate.update(dt);
    if (m_fadeStage == FadeStage::None && m_levelCompleteTimer >= kLevelCompleteMinTime) m_fadeStage = FadeStage::FadeOut;
}

void Game::updateFade(float dt) {
    if (m_fadeStage == FadeStage::FadeOut) {
        m_fadeAlpha += dt / kFadeTime;
        if (m_fadeAlpha >= 1.f) {
            m_fadeAlpha = 1.f;
            advanceToNextLevel();
            m_fadeStage = FadeStage::FadeIn;
        }
    } else if (m_fadeStage == FadeStage::FadeIn) {
        m_fadeAlpha -= dt / kFadeTime;
        if (m_fadeAlpha <= 0.f) {
            m_fadeAlpha = 0.f;
            m_fadeStage = FadeStage::None;
        }
    }
}

void Game::advanceToNextLevel() {
    loadLevel(std::min(m_levelId + 1, kLevelCount));
    m_progress.currentLevel = m_levelId;
    setState(GameState::Playing);
}

void Game::enterGameOver() {
    if (m_gameOverHandled) return;
    m_gameOverHandled = true;
    flushTotals();
    saveProgress();
    m_dispatcher.dispatch(GameEvent::GameOver);
    setState(GameState::GameOver);
}

void Game::goToMenu() {
    saveOnExit();
    m_effects.clear();
    m_projectiles.clear();
    m_items.clear();
    enterMenuWithProgress();
}

// ============================================================================
//  Cập nhật khi đang chơi
// ============================================================================
PlayerInput Game::readInput() {
    if (m_autopilot) return autopilotInput();

    PlayerInput input;
    if (!m_window.hasFocus()) return input;

    using Key = sf::Keyboard::Key;
    if (sf::Keyboard::isKeyPressed(Key::A) || sf::Keyboard::isKeyPressed(Key::Left)) input.move.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(Key::D) || sf::Keyboard::isKeyPressed(Key::Right)) input.move.x += 1.f;
    if (sf::Keyboard::isKeyPressed(Key::W) || sf::Keyboard::isKeyPressed(Key::Up)) input.move.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(Key::S) || sf::Keyboard::isKeyPressed(Key::Down)) input.move.y += 1.f;
    input.run = sf::Keyboard::isKeyPressed(Key::LShift) || sf::Keyboard::isKeyPressed(Key::RShift);
    input.attack = sf::Keyboard::isKeyPressed(Key::J) || sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    input.dash = sf::Keyboard::isKeyPressed(Key::Space) || sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    return input;
}

void Game::updatePlaying(float dt) {
    m_survivalTime += dt;
    m_levelUpTimer = std::max(0.f, m_levelUpTimer - dt);

    m_audio.update(dt);
    m_player.updateWithCombat(dt, readInput(), m_map, m_projectiles, m_effects);
    if (m_autopilotGod) m_player.heal(1000.f);

    if (!m_player.isDead()) updateSpawner(dt);

    m_pathTimer -= dt;
    if (m_pathTimer <= 0.f) {
        m_pathToPlayer.build(m_map, m_player.getPosition());
        m_pathTimer = 0.2f;
    }

    for (const std::unique_ptr<Monster>& monster : m_monsters) {
        monster->updateWithPlayer(dt, m_player, m_map, m_pathToPlayer, m_projectiles);
        const int dotDamage = monster->consumeDotDamage();
        if (dotDamage > 0) m_effects.addText(monster->getBodyCenter() + sf::Vector2f(0.f, -34.f), std::to_string(dotDamage), sf::Color(255, 170, 70), 18);

        // Hào quang thiêu đốt nếu người chơi đã nâng cấp
        if (m_player.hasBurnAura() && !monster->isDead()) {
            if (util::distance(m_player.getPosition(), monster->getPosition()) < 110.f) {
                monster->applyDot(1.2f, 0.4f, 10.f);
            }
        }
    }

    separateMonsters();
    resolvePlayerAttack();
    resolveMonsterAttacks();
    updateProjectiles(dt);
    resolveProjectiles();
    updateItems(dt);
    collectDeathRewards();
    removeDeadMonsters();

    m_effects.update(dt);
    m_effects.updateAmbient(dt);
    m_gate.update(dt);
    if (!m_player.isDead()) updateLevelFlow(dt);
    updateCamera(dt, false);

    if (m_player.isDead() && m_state == GameState::Playing) {
        m_gameOverDelay += dt;
        if (m_gameOverDelay >= 1.3f) enterGameOver();
    }
}

int Game::countAliveMonsters() const {
    int count = 0;
    for (const std::unique_ptr<Monster>& monster : m_monsters)
        if (!monster->isDead()) ++count;
    return count;
}

const Monster* Game::findBoss() const {
    for (const std::unique_ptr<Monster>& monster : m_monsters)
        if (monster->isBoss() && !monster->isDead()) return monster.get();
    return nullptr;
}

void Game::updateSpawner(float dt) {
    if (m_spawnedCount >= m_levelTotalToSpawn) return;
    const LevelConfig& config = getLevelConfig(m_levelId);

    m_spawnTimer -= dt;
    if (m_spawnTimer > 0.f) return;
    const int alive = countAliveMonsters();

    if (config.bossAtEnd && m_spawnedCount >= config.totalMonsters) {
        m_spawnTimer = 0.5f;
        if (alive <= 1 && spawnMonster(MonsterType::SlimeKing)) {
            ++m_spawnedCount;
            showToast("BOSS xuất hiện: SLIME KING!", 3.f);
            m_dispatcher.dispatch(GameEvent::BossSpawned);
        }
        return;
    }

    m_spawnTimer = config.spawnInterval;
    if (alive >= config.maxAlive) return;
    if (spawnMonster(chooseMonsterType())) ++m_spawnedCount;
}

MonsterType Game::chooseMonsterType() const {
    const LevelConfig& config = getLevelConfig(m_levelId);
    const float r = util::randomFloat(0.f, 1.f);

    // Khi chạy autopilot smoke test: dùng cấu hình quái gốc để đường đi BFS của autopilot không kẹt
    if (m_autopilot) {
        if (r < config.tankChance) return MonsterType::SlimeTank;
        if (r < config.tankChance + config.purpleChance) return MonsterType::SlimePurple;
        return MonsterType::SlimeGreen;
    }

    // Chế độ chơi thật của người chơi: đa dạng quái vật với Skeleton, Bat, Tank Slime
    if (m_levelId == 1) {
        if (r < 0.25f) return MonsterType::Bat;
        return MonsterType::SlimeGreen;
    } else if (m_levelId == 2) {
        if (r < 0.25f) return MonsterType::Bat;
        if (r < 0.25f + config.purpleChance) return MonsterType::SlimePurple;
        return MonsterType::SlimeGreen;
    } else if (m_levelId == 3) {
        if (r < 0.30f) return MonsterType::Skeleton;
        if (r < 0.30f + config.tankChance) return MonsterType::SlimeTank;
        if (r < 0.30f + config.tankChance + config.purpleChance) return MonsterType::SlimePurple;
        return MonsterType::SlimeGreen;
    } else if (m_levelId == 4) {
        if (r < 0.25f) return MonsterType::Bat;
        if (r < 0.50f) return MonsterType::Skeleton;
        if (r < 0.50f + config.tankChance) return MonsterType::SlimeTank;
        return MonsterType::SlimePurple;
    } else {
        if (r < 0.30f) return MonsterType::Skeleton;
        if (r < 0.30f + config.tankChance) return MonsterType::SlimeTank;
        return MonsterType::SlimePurple;
    }
}

bool Game::findSpawnPosition(float radius, sf::Vector2f& out) const {
    const LevelConfig& config = getLevelConfig(m_levelId);
    const bool hasZone = config.spawnZone.size.x > 0.f && config.spawnZone.size.y > 0.f;
    const sf::FloatRect zone = hasZone ? config.spawnZone : sf::FloatRect({0.f, 0.f}, m_map.getSize());
    const sf::Vector2f playerPos = m_player.getPosition();

    auto acceptable = [&](sf::Vector2f p, float minPlayerDistance) {
        return zone.contains(p) && m_map.isCircleFree(p, radius) && util::distance(p, playerPos) >= minPlayerDistance &&
               util::distance(p, config.exit) >= 260.f;
    };

    for (int attempt = 0; attempt < 40; ++attempt) {
        const float angle = util::randomFloat(0.f, kPi2);
        const float distance = util::randomFloat(560.f, 820.f);
        const sf::Vector2f candidate = playerPos + sf::Vector2f(std::cos(angle), std::sin(angle)) * distance;
        if (acceptable(candidate, 0.f)) {
            out = candidate;
            return true;
        }
    }
    for (const float minDistance : {450.f, 300.f, 200.f}) {
        for (int attempt = 0; attempt < 50; ++attempt) {
            const sf::Vector2f candidate(util::randomFloat(zone.position.x, zone.position.x + zone.size.x),
                                         util::randomFloat(zone.position.y, zone.position.y + zone.size.y));
            if (acceptable(candidate, minDistance)) {
                out = candidate;
                return true;
            }
        }
    }
    return false;
}

bool Game::spawnMonster(MonsterType type) {
    const float radius = (type == MonsterType::SlimeKing) ? 34.f : ((type == MonsterType::SlimeTank || type == MonsterType::Skeleton) ? 18.f : 16.f);
    sf::Vector2f position;
    if (!findSpawnPosition(radius, position)) return false;
    const balance::MonsterScale scale = balance::monsterScaleForLevel(m_levelId);
    m_monsters.push_back(MonsterFactory::createMonster(type, position, scale, m_assets));
    m_effects.addSparks(position + sf::Vector2f(0.f, -20.f), sf::Color(220, 200, 255), 8);
    return true;
}

void Game::separateMonsters() {
    for (std::size_t i = 0; i < m_monsters.size(); ++i) {
        Monster& a = *m_monsters[i];
        if (a.isDead()) continue;

        {
            const sf::Vector2f offset = a.getPosition() - m_player.getPosition();
            const float dist = util::length(offset);
            const float minDist = a.getFootRadius() + m_player.getFootRadius();
            if (dist < minDist) {
                const sf::Vector2f outward = dist > 0.001f ? offset / dist : sf::Vector2f(1.f, 0.f);
                a.nudge(outward * (minDist - dist), m_map);
            }
        }
        for (std::size_t j = i + 1; j < m_monsters.size(); ++j) {
            Monster& b = *m_monsters[j];
            if (b.isDead()) continue;
            const sf::Vector2f offset = b.getPosition() - a.getPosition();
            const float dist = util::length(offset);
            const float minDist = a.getFootRadius() + b.getFootRadius();
            if (dist < minDist) {
                const sf::Vector2f outward = dist > 0.001f ? offset / dist : sf::Vector2f(1.f, 0.f);
                const sf::Vector2f push = outward * ((minDist - dist) * 0.5f);
                a.nudge(-push, m_map);
                b.nudge(push, m_map);
            }
        }
    }
}

void Game::resolvePlayerAttack() {
    if (!m_player.hasPendingHit()) return;

    m_dispatcher.dispatch(GameEvent::PlayerAttack);
    const balance::SlashProfile& slash = m_player.getSlashProfile();
    const sf::Color slashColor(slash.r, slash.g, slash.b);
    const sf::FloatRect hitbox = m_player.getAttackHitbox();

    for (const std::unique_ptr<Monster>& monster : m_monsters) {
        if (monster->isDead()) continue;
        if (!hitbox.findIntersection(monster->getHitbox()).has_value()) continue;

        float damage = m_player.getDamage() * slash.damageMultiplier;
        const bool isCrit = util::randomFloat(0.f, 1.f) < m_player.getCritChance();
        if (isCrit) {
            damage *= 2.0f;
            m_effects.addText(monster->getBodyCenter() + sf::Vector2f(0.f, -50.f), "CHÍ MẠNG!", sf::Color(255, 60, 60), 24);
        }

        const bool killed = monster->takeDamage(damage, m_player.getPosition());
        m_effects.addSparks(monster->getBodyCenter(), slashColor, killed ? 14 : 7);
        m_effects.addText(monster->getBodyCenter() + sf::Vector2f(0.f, -30.f),
                          std::to_string(static_cast<int>(std::lround(damage))),
                          isCrit ? sf::Color(255, 230, 80) : sf::Color(255, 245, 190), 22);

        if (!killed) {
            if (slash.slowDuration > 0.f) monster->applySlow(slash.slowDuration, slash.slowMultiplier);
            if (slash.dotDuration > 0.f) {
                const float tickDamage = std::max(1.f, std::round(m_player.getDamage() * slash.dotDamageFactor));
                monster->applyDot(slash.dotDuration, slash.dotTickInterval, tickDamage);
            }
        }
    }
    m_player.confirmHit();
}

void Game::resolveMonsterAttacks() {
    for (const std::unique_ptr<Monster>& monster : m_monsters) {
        if (!monster->consumeAttackHit()) continue;
        const float dist = util::distance(monster->getPosition(), m_player.getPosition());
        if (dist > monster->getAttackRange() * 1.35f) continue;
        if (m_player.takeDamage(monster->getDamage(), monster->getPosition())) {
            m_dispatcher.dispatch(GameEvent::PlayerDamaged);
            const sf::Vector2f chest = m_player.getPosition() + sf::Vector2f(0.f, -50.f);
            m_effects.addText(chest + sf::Vector2f(0.f, -30.f), "-" + std::to_string(static_cast<int>(std::lround(monster->getDamage()))), sf::Color(255, 90, 80), 24);
            m_effects.addSparks(chest, sf::Color(255, 90, 80), 6);
        }
    }
}

void Game::updateProjectiles(float dt) {
    for (auto& proj : m_projectiles) {
        proj.update(dt, m_player.getPosition(), m_map);
    }
    m_projectiles.erase(std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                                       [](const Projectile& p) { return p.isDead(); }),
                        m_projectiles.end());
}

void Game::resolveProjectiles() {
    for (auto& proj : m_projectiles) {
        if (proj.isDead()) continue;

        if (proj.isFromPlayer()) {
            for (auto& monster : m_monsters) {
                if (monster->isDead()) continue;
                if (proj.checkHit(monster->getHitbox())) {
                    const bool killed = monster->takeDamage(proj.getDamage(), proj.getPosition());
                    m_effects.addSparks(monster->getBodyCenter(), sf::Color(210, 160, 255), killed ? 14 : 7);
                    m_effects.addText(monster->getBodyCenter() + sf::Vector2f(0.f, -30.f),
                                      std::to_string(static_cast<int>(std::lround(proj.getDamage()))),
                                      sf::Color(230, 200, 255), 20);
                    m_dispatcher.dispatch(GameEvent::PlayerAttack);
                    if (proj.getType() != ProjectileType::Boomerang) {
                        proj.destroy();
                        break;
                    }
                }
            }
        } else {
            if (proj.checkHit(m_player.getHitbox())) {
                if (m_player.takeDamage(proj.getDamage(), proj.getPosition())) {
                    m_dispatcher.dispatch(GameEvent::PlayerDamaged);
                    const sf::Vector2f chest = m_player.getPosition() + sf::Vector2f(0.f, -50.f);
                    m_effects.addText(chest + sf::Vector2f(0.f, -30.f),
                                      "-" + std::to_string(static_cast<int>(std::lround(proj.getDamage()))),
                                      sf::Color(255, 90, 80), 24);
                    m_effects.addSparks(chest, sf::Color(255, 90, 80), 6);
                }
                proj.destroy();
            }
        }
    }
}

void Game::updateItems(float dt) {
    const float magnetR = m_player.getMagnetRadius();
    for (auto& item : m_items) {
        item->update(dt, m_player.getPosition(), magnetR);
        if (item->checkCollect(m_player.getPosition(), m_player.getFootRadius())) {
            item->apply(m_player);
            m_dispatcher.dispatch(GameEvent::ItemCollected);

            if (item->getType() == ItemType::Bomb) {
                m_dispatcher.dispatch(GameEvent::BombExploded);
                m_effects.addSparks(item->getPosition(), sf::Color(255, 120, 40), 40);
                for (auto& m : m_monsters) {
                    if (!m->isDead()) {
                        m->takeDamage(130.f, item->getPosition());
                    }
                }
            }
        }
    }
    m_items.erase(std::remove_if(m_items.begin(), m_items.end(),
                                 [](const std::unique_ptr<Item>& it) { return it->isCollected(); }),
                  m_items.end());
}

void Game::collectDeathRewards() {
    for (const std::unique_ptr<Monster>& monster : m_monsters) {
        if (!monster->consumeDeathReward()) continue;
        m_score += monster->getScoreValue();
        ++m_kills;
        ++m_levelKills;
        m_unsavedScore += monster->getScoreValue();
        ++m_unsavedKills;

        m_dispatcher.dispatch(GameEvent::MonsterKilled);

        if (auto drop = ItemFactory::createRandomDrop(monster->getPosition(), 0.45f)) {
            m_items.push_back(std::move(drop));
        }

        if (m_player.addExp(monster->getExpValue())) {
            m_levelUpTimer = 2.0f;
            m_dispatcher.dispatch(GameEvent::PlayerLeveledUp);
            if (!m_autopilot) {
                triggerPowerUpSelection();
            } else {
                auto cards = PowerUpManager::getRandomCards(3);
                for (const auto& card : cards) {
                    if (card.type != PowerUpType::WeaponWand && card.type != PowerUpType::WeaponBoomerang) {
                        PowerUpManager::applyCard(m_player, card);
                        break;
                    }
                }
            }
        }
    }
}

void Game::removeDeadMonsters() {
    m_monsters.erase(std::remove_if(m_monsters.begin(), m_monsters.end(),
                                    [](const std::unique_ptr<Monster>& m) { return m->shouldRemove(); }),
                     m_monsters.end());
}

void Game::updateCamera(float dt, bool snap) {
    const sf::Vector2f target = m_player.getPosition() + sf::Vector2f(0.f, -40.f);
    const float k = snap ? 1.f : 1.f - std::exp(-8.f * dt);
    m_cameraPos += (target - m_cameraPos) * k;

    const sf::Vector2f viewSize = m_camera.getSize();
    const sf::Vector2f mapSize = m_map.getSize();
    sf::Vector2f center = m_cameraPos;
    center.x = (mapSize.x <= viewSize.x) ? mapSize.x / 2.f : util::clampf(center.x, viewSize.x / 2.f, mapSize.x - viewSize.x / 2.f);
    center.y = (mapSize.y <= viewSize.y) ? mapSize.y / 2.f : util::clampf(center.y, viewSize.y / 2.f, mapSize.y - viewSize.y / 2.f);
    m_camera.setCenter({std::round(center.x), std::round(center.y)});
}

void Game::triggerPowerUpSelection() {
    m_powerUpChoices = PowerUpManager::getRandomCards(3);
    setState(GameState::PowerUpSelect);
}

void Game::selectPowerUp(int index) {
    if (index >= 0 && index < static_cast<int>(m_powerUpChoices.size())) {
        PowerUpManager::applyCard(m_player, m_powerUpChoices[index]);
        showToast("Đã chọn nâng cấp: " + m_powerUpChoices[index].title, 2.5f);
        m_audio.playPickup();
        setState(GameState::Playing);
    }
}

void Game::openLeaderboard() {
    if (m_state != GameState::Leaderboard) {
        m_leaderboardPrevState = m_state;
    }
    m_topScores = ShopSystem::getTopScores(m_repo, 5);
    setState(GameState::Leaderboard);
}

void Game::openShop() {
    m_shopItems = ShopSystem::getShopItems(m_progress);
    m_selectedShopIndex = 0;
    setState(GameState::Shop);
}
