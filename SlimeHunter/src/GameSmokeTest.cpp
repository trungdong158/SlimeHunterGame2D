// GameSmokeTest.cpp - Chế độ TỰ KIỂM THỬ (không dùng khi chơi bình thường):  SlimeHunter --smoke-test [tiền_tố_ảnh]
// Một "autopilot" tự chơi qua cả 5 màn, rồi kiểm tra các tình huống lỗi. Mọi dữ liệu CSV nằm ở thư mục TẠM.

#include "Game.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>

// Chống kẹt cho autopilot (giống Monster::moveToward): đi mà không nhích được > 0.4 giây thì né sang bên 0.4 giây.
static PlayerInput withUnstick(PlayerInput input, sf::Vector2f position, sf::Vector2f& lastPos, float& stuckTime, float& sidestepTime, sf::Vector2f& sidestepDir) {
    const float dt = 1.f / 60.f;
    const bool wantsToMove = util::length(input.move) > 0.f;
    if (sidestepTime > 0.f) {
        sidestepTime -= dt;
        input.move = sidestepDir;
    }
    if (wantsToMove && util::distance(position, lastPos) < 230.f * dt * 0.25f) {
        stuckTime += dt;
        if (stuckTime > 0.4f) {
            stuckTime = 0.f;
            sidestepTime = 0.4f;
            const float angle = util::randomFloat(1.0f, 2.1f) * (util::randomInt(0, 1) == 0 ? 1.f : -1.f);
            const sf::Vector2f d = util::normalized(input.move);
            sidestepDir = {d.x * std::cos(angle) - d.y * std::sin(angle), d.x * std::sin(angle) + d.y * std::cos(angle)};
        }
    } else {
        stuckTime = 0.f;
    }
    lastPos = position;
    return input;
}

PlayerInput Game::autopilotInput() {
    PlayerInput input;
    if (m_autopilotPassive) return input;  // đứng yên để bị quái hạ gục (kiểm tra Game Over)

    // Mỗi lần gọi ứng với một khung hình 1/60 giây
    m_autopilotPathTimer -= 1.f / 60.f;

    if (m_phase == LevelPhase::GateOpen) {  // đã mở cổng: đi tới cổng (BFS quanh vật cản)
        if (m_autopilotPathTimer <= 0.f) {
            m_autopilotPath.build(m_map, m_gate.getPosition());
            m_autopilotPathTimer = 0.2f;
        }
        if (m_map.isLineFree(m_player.getPosition(), m_gate.getPosition(), m_player.getFootRadius())) input.move = m_gate.getPosition() - m_player.getPosition();
        else input.move = m_autopilotPath.getDirection(m_map, m_player.getPosition(), m_player.getFootRadius());
        return withUnstick(input, m_player.getPosition(), m_autoLastPos, m_autoStuckTime, m_autoSidestepTime, m_autoSidestepDir);
    }

    const Monster* nearest = nullptr;
    float best = 1.0e9f;
    for (const std::unique_ptr<Monster>& monster : m_monsters) {
        if (monster->isDead()) continue;
        const float dist = util::distance(monster->getPosition(), m_player.getPosition());
        if (dist < best) {
            best = dist;
            nearest = monster.get();
        }
    }
    if (nearest != nullptr) {
        if (best > 75.f) {
            if (m_autopilotPathTimer <= 0.f) {
                m_autopilotPath.build(m_map, nearest->getPosition());
                m_autopilotPathTimer = 0.2f;
            }
            if (m_map.isLineFree(m_player.getPosition(), nearest->getPosition(), m_player.getFootRadius())) input.move = nearest->getPosition() - m_player.getPosition();
            else input.move = m_autopilotPath.getDirection(m_map, m_player.getPosition(), m_player.getFootRadius());
        }
        input.attack = best < 130.f;
    }
    return withUnstick(input, m_player.getPosition(), m_autoLastPos, m_autoStuckTime, m_autoSidestepTime, m_autoSidestepDir);
}

int Game::runSmokeTest(const std::string& prefix) {
    namespace fs = std::filesystem;
    const float dt = 1.f / 60.f;
    int failures = 0;
    int checks = 0;
    auto check = [&](bool ok, const std::string& what) {
        ++checks;
        std::cout << (ok ? "[ OK ] " : "[FAIL] ") << what << '\n';
        if (!ok) ++failures;
    };
    auto step = [&](bool draw) {
        processEvents();
        update(dt);
        if (draw) render();
    };
    auto shot = [&](const std::string& name) {
        if (!prefix.empty()) m_pendingScreenshot = prefix + name + ".png";
    };
    auto snapshot = [&](const std::string& name) {
        shot(name);
        render();
    };
    auto press = [&](sf::Keyboard::Key key) { handleKeyPressed(key); };

    m_autopilot = true;
    UserRepository disk(m_repo.getDataDir());  // một "người đọc độc lập" để xác nhận dữ liệu THỰC SỰ nằm trong file CSV
    auto onDisk = [&]() { return disk.loadProgress(m_user.has_value() ? m_user->userId : -1); };

    // =======================================================================
    std::cout << "--- 1. Dang nhap / dang ky / tien trinh mac dinh ---\n";
    check(m_state == GameState::Login, "Khoi dong o man hinh Dang nhap");
    m_loginUser = "tes";
    m_loginPass = "secret99";
    snapshot("0_login");
    // Nhập liệu bằng sự kiện văn bản (cùng đường đi với bàn phím thật)
    m_loginUser.clear();
    m_loginField = 0;
    handleTextEntered(U'a');
    handleTextEntered(U'b');
    handleTextEntered(U'c');
    handleTextEntered(8);  // Backspace
    handleTextEntered(U' ');  // khoảng trắng bị bỏ qua
    handleTextEntered(U'\n');  // ký tự điều khiển bị bỏ qua
    handleTextEntered(U'\u1EA1');  // chữ có dấu tiếng Việt bị bỏ qua (username chỉ nhận ASCII)
    check(m_loginUser == "ab", "Nhap lieu: ky tu thuong duoc nhan, Backspace xoa, khoang trang/ky tu dieu khien/chu co dau bi bo qua");
    for (int i = 0; i < 50; ++i) handleTextEntered(U'x');
    check(m_loginUser.size() == 32, "Nhap lieu: gioi han 32 ky tu");
    press(sf::Keyboard::Key::Tab);
    check(m_loginField == 1, "Tab: chuyen sang o Mat khau");
    press(sf::Keyboard::Key::Tab);
    check(m_loginField == 0, "Tab lan 2: ve o Ten dang nhap");

    // Kiem tra nut con mat an/hien mat khau
    check(!m_showPassword, "Con mat mat khau: mac dinh la an (*)");
    const float testBoxW = 480.f;
    const float testBoxX = (m_uiView.getSize().x - testBoxW) / 2.f;
    handleLoginClick({testBoxX + testBoxW - 24.f, 354.f});
    check(m_showPassword && m_loginField == 1, "Click con mat: hien mat khau va giu focus o mat khau");
    handleLoginClick({testBoxX + testBoxW - 24.f, 354.f});
    check(!m_showPassword, "Click con mat lan 2: an mat khau ve (*)");
    m_loginUser = "tes";
    m_registerMode = true;
    m_loginUser = "a b";
    m_loginPass = "pass1234";
    submitLoginForm();
    check(m_state == GameState::Login && !m_loginMessage.empty(), "Username sai dinh dang (co khoang trang) bi tu choi");
    m_loginUser = "tester";
    m_loginPass = "ab";
    submitLoginForm();
    check(m_state == GameState::Login, "Mat khau qua ngan bi tu choi");
    m_loginUser = "tester";
    m_loginPass = "pass1234";
    submitLoginForm();
    check(m_state == GameState::Menu && m_user.has_value() && m_user->username == "tester", "Tao tai khoan moi va vao Menu");
    check(m_loginPass.empty(), "Mat khau duoc xoa khoi o nhap sau khi dang nhap");
    check(m_progress.maxLevelUnlocked == 1 && m_progress.playerMaxHp == 100 && m_progress.playerDamage == 20, "Tien trinh mac dinh: man 1, 100 HP, 20 sat thuong");
    snapshot("1_menu");
    press(sf::Keyboard::Key::Right);
    check(m_selectedLevel == 1, "Chua mo khoa man 2: khong chon duoc man 2");

    press(sf::Keyboard::Key::L);
    check(m_state == GameState::Login && !m_user.has_value(), "Dang xuat ve man hinh Dang nhap");
    m_registerMode = true;
    m_loginUser = "TESTER";
    m_loginPass = "other123";
    submitLoginForm();
    check(m_state == GameState::Login, "Dang ky trung username (khac hoa/thuong) bi tu choi");
    m_registerMode = false;
    m_loginUser = "tester";
    m_loginPass = "wrongpass";
    submitLoginForm();
    check(m_state == GameState::Login, "Dang nhap sai mat khau bi tu choi");
    m_loginPass = "pass1234";
    submitLoginForm();
    check(m_state == GameState::Menu && m_user.has_value(), "Dang nhap dung vao Menu");

    // =======================================================================
    std::cout << "--- 2. Kiem tra du lieu 5 level (spawn/exit hop le, co duong di) ---\n";
    for (int id = 1; id <= kLevelCount; ++id) {
        const LevelConfig& config = getLevelConfig(id);
        m_map.loadLevel(m_assets, id);
        PathField field;
        field.build(m_map, config.exit);
        const std::string tag = "Level " + std::to_string(id) + " (" + config.name + "): ";
        check(m_map.isCircleFree(config.spawn, 18.f), tag + "diem xuat hien nam tren dat trong");
        check(m_map.isCircleFree(config.exit, 24.f), tag + "loi ra nam tren dat trong, khong trong vat can");
        check(config.exit.x > config.exitRadius && config.exit.y > config.exitRadius && config.exit.x < m_map.getSize().x - config.exitRadius &&
                  config.exit.y < m_map.getSize().y - config.exitRadius,
              tag + "cong thoat nam tron trong gioi han ban do");
        check(field.getDistance(config.spawn) >= 0, tag + "co duong di tu spawn toi exit (BFS)");
        check(util::distance(config.spawn, config.exit) > 300.f, tag + "exit du xa diem xuat hien");
    }
    check(balance::monsterScaleForLevel(5).hp > balance::monsterScaleForLevel(2).hp && balance::monsterScaleForLevel(5).damage > balance::monsterScaleForLevel(1).damage &&
              balance::monsterScaleForLevel(99).speed <= balance::kMonsterSpeedMax + 0.0001f && balance::monsterScaleForLevel(99).cooldown >= balance::kMonsterCooldownMin - 0.0001f,
          "Quai manh dan theo level; toc do/hoi chieu co gioi han");
    check(balance::slashProfileForTier(1).slowDuration == 0.f && balance::slashProfileForTier(1).dotDuration == 0.f && balance::slashProfileForTier(3).slowDuration > 0.f &&
              balance::slashProfileForTier(4).dotDuration > 0.f && balance::slashProfileForTier(5).slowDuration > balance::slashProfileForTier(3).slowDuration,
          "Hieu ung chem mo khoa dan: T1 co ban, T3 lam cham, T4 thieu dot, T5 nang cap");

    // =======================================================================
    int expectedHp = 100;
    int expectedDamage = 20;
    int expectedUnlocked = 1;
    long long killsMax = 0;

    // Chơi một màn bằng autopilot cho tới khi qua màn. firstClear = đây có phải lần đầu qua màn mới nhất không.
    auto runLevel = [&](int level, bool firstClear, bool earlyGateTest) {
        const std::string tag = "Level " + std::to_string(level) + ": ";
        const LevelConfig& config = getLevelConfig(level);
        std::cout << "--- Choi " << tag << config.name << " ---\n";
        check(m_state == GameState::Playing && m_levelId == level && m_phase == LevelPhase::Fighting && !m_gate.isOpen() && m_monsters.empty(),
              tag + "bat dau: dang choi, cong DONG, khong co quai cu");
        check(util::distance(m_player.getPosition(), config.spawn) < 60.f, tag + "Player xuat hien dung diem spawn cua man");

        if (earlyGateTest) {  // chạm cổng khi chưa diệt hết quái: KHÔNG được qua màn
            const sf::Vector2f back = m_player.getPosition();
            m_player.teleportTo(config.exit);
            for (int f = 0; f < 40; ++f) step(false);
            check(m_state == GameState::Playing && m_phase == LevelPhase::Fighting && m_levelId == level, tag + "cham cong khi chua diet het quai: KHONG qua man");
            check(m_gateHintTimer > 0.f, tag + "hien goi y 'hay tieu diet toan bo quai de mo cong'");
            check(onDisk().maxLevelUnlocked == expectedUnlocked, tag + "chua luu gi khi cham cong som");
            m_player.teleportTo(back);
        }

        // Giai đoạn 1: chiến đấu tới khi cổng mở
        bool shotFight = false, shotSlash = false;
        int maxAlive = 0;
        bool opened = false;
        for (int f = 0; f < 12000 && m_state == GameState::Playing; ++f) {
            maxAlive = std::max(maxAlive, countAliveMonsters());
            bool want = (f % 45 == 0);
            if (!shotFight && f == 800) { shot("L" + std::to_string(level) + "_fight"); shotFight = want = true; }
            if (!shotSlash && f > 500 && m_player.isAttacking() && m_effects.activeCount() > 2) { shot("L" + std::to_string(level) + "_slash"); shotSlash = want = true; }
            step(want);
            if (m_phase == LevelPhase::GateOpen) { opened = true; break; }
        }
        check(opened, tag + "dien het quai cua dot -> cong mo");
        if (!opened) return false;
        check(countAliveMonsters() == 0 && m_spawnedCount == m_levelTotalToSpawn && m_levelKills == m_levelTotalToSpawn,
              tag + "cong chi mo khi da sinh du va diet het " + std::to_string(m_levelTotalToSpawn) + " quai");
        check(maxAlive <= config.maxAlive + 1, tag + "khong vuot gioi han quai cung luc (" + std::to_string(maxAlive) + " <= " + std::to_string(config.maxAlive) + ")");
        const int spawnedAtOpen = m_spawnedCount;
        for (int f = 0; f < 120; ++f) step(false);  // đợi thêm: không được sinh thêm quái sau khi đợt đã hoàn thành
        check(m_spawnedCount == spawnedAtOpen && countAliveMonsters() == 0, tag + "dung sinh quai sau khi dot hoan thanh");
        if (level == 3 || level == 5) check(config.spawnZone.size.x > 0.f, tag + "co vung sinh quai rieng");

        // Giai đoạn 2: đi tới cổng
        bool shotGate = false;
        for (int f = 0; f < 4000 && m_state == GameState::Playing; ++f) {
            bool want = (f % 60 == 0);
            if (!shotGate && util::distance(m_player.getPosition(), m_gate.getPosition()) < 330.f) { shot("L" + std::to_string(level) + "_gate"); shotGate = want = true; }
            step(want);
        }
        const GameState expectedState = (level == kLevelCount) ? GameState::Victory : GameState::LevelComplete;
        check(m_state == expectedState, tag + "cham cong da mo -> hoan thanh man");
        if (m_state != expectedState) return false;

        // Phần thưởng và lưu CSV
        if (firstClear) {
            expectedHp = balance::upgradedMaxHp(expectedHp);
            expectedDamage = balance::upgradedDamage(expectedDamage);
            expectedUnlocked = std::min(level + 1, kLevelCount + 1);
        }
        const ProgressRecord saved = onDisk();
        check(saved.maxLevelUnlocked == expectedUnlocked && saved.playerMaxHp == expectedHp && saved.playerDamage == expectedDamage,
              tag + "CSV: mo khoa man " + std::to_string(expectedUnlocked) + ", " + std::to_string(expectedHp) + " HP, " + std::to_string(expectedDamage) + " sat thuong" +
                  (firstClear ? " (nang cap +15%)" : " (qua lai man cu: KHONG nang cap)"));
        check(m_player.getBaseMaxHp() == expectedHp && m_player.getBaseDamage() == expectedDamage && m_player.getHp() == m_player.getMaxHp(),
              tag + "Player: chi so moi, thanh mau cap nhat, hoi day mau");
        check(m_player.getSlashTier() == balance::slashTierForProgress(expectedUnlocked), tag + "hieu ung chem mo khoa theo tien trinh (tier " + std::to_string(m_player.getSlashTier()) + ")");
        check(saved.totalKills >= killsMax && m_unsavedKills == 0, tag + "tong quai da luu, bo dem chua luu ve 0");
        killsMax = saved.totalKills;

        // Gọi hoàn thành nhiều lần: KHÔNG được cộng lặp
        for (int i = 0; i < 6; ++i) completeLevel();
        const ProgressRecord again = onDisk();
        check(again.maxLevelUnlocked == expectedUnlocked && again.playerMaxHp == expectedHp && again.playerDamage == expectedDamage &&
                  m_progress.playerMaxHp == expectedHp,
              tag + "goi hoan thanh man 6 lan nua: phan thuong KHONG bi cong lap");
        snapshot("L" + std::to_string(level) + (level == kLevelCount ? "_victory" : "_complete"));

        if (level < kLevelCount) {  // chuyển cảnh sang màn tiếp theo
            for (int f = 0; f < 600 && m_state != GameState::Playing; ++f) step(false);
            check(m_state == GameState::Playing && m_levelId == level + 1, tag + "chuyen sang man " + std::to_string(level + 1));
            check(m_monsters.empty() && m_effects.activeCount() == 0 && m_phase == LevelPhase::Fighting && !m_gate.isOpen() && m_spawnedCount == 0,
                  tag + "quai, hieu ung, bo dem sinh quai, cong cua man cu duoc don sach");
            check(util::distance(m_player.getPosition(), getLevelConfig(level + 1).spawn) < 60.f, tag + "Player xuat hien dung diem spawn cua man moi");
            check(m_player.getBaseMaxHp() == expectedHp, tag + "Player khong bi reset khi chuyen man");
        }
        return true;
    };

    // =======================================================================
    std::cout << "--- 3. Choi 5 man lien tiep ---\n";
    m_autopilotGod = true;
    press(sf::Keyboard::Key::Enter);
    runLevel(1, true, true);
    for (int level = 2; level <= kLevelCount; ++level) {
        if (m_state != GameState::Playing) break;
        runLevel(level, true, false);
    }
    check(m_state == GameState::Victory, "Hoan thanh man cuoi -> man hinh CHIEN THANG");
    check(onDisk().maxLevelUnlocked == kLevelCount + 1, "CSV: da thong quan (max_level_unlocked = " + std::to_string(kLevelCount + 1) + ")");

    // =======================================================================
    std::cout << "--- 4. Hieu ung lam cham va sat thuong theo thoi gian ---\n";
    m_map.loadLevel(m_assets, 1);
    const sf::Vector2f origin = m_map.findFreeSpotNear(getLevelConfig(1).spawn, 20.f);
    m_player.teleportTo(origin);
    m_pathToPlayer.build(m_map, origin);
    const balance::MonsterScale unit;
    {
        sf::Vector2f far = origin + sf::Vector2f(260.f, 0.f);
        far = m_map.findFreeSpotNear(far, 20.f);
        Slime plain(m_assets, SlimeType::Green, far, unit);
        Slime slowed(m_assets, SlimeType::Green, far, unit);
        slowed.applySlow(2.0f, 0.5f);
        check(slowed.isSlowed() && std::fabs(slowed.getSlowFactor() - 0.5f) < 0.001f, "Slow: dang bi lam cham, he so 0.5");
        slowed.applySlow(1.0f, 0.8f);
        check(std::fabs(slowed.getSlowFactor() - 0.5f) < 0.001f && slowed.getSlowTimer() > 1.9f, "Slow lan 2 (yeu hon): khong cong don, khong rut ngan hieu ung dang co");
        Slime extreme(m_assets, SlimeType::Green, far, unit);
        extreme.applySlow(1.0f, -5.f);
        check(extreme.getSlowFactor() >= 0.2f && extreme.getSlowFactor() > 0.f, "Slow: toc do khong bao gio am hoac bang 0");
        for (int f = 0; f < 60; ++f) {
            plain.update(dt, m_player, m_map, m_pathToPlayer);
            slowed.update(dt, m_player, m_map, m_pathToPlayer);
        }
        const float movedPlain = util::distance(plain.getPosition(), far);
        const float movedSlow = util::distance(slowed.getPosition(), far);
        check(movedPlain > 5.f && movedSlow < movedPlain * 0.8f, "Slow: quai bi cham di duoc it hon han quai binh thuong trong 1 giay");
        for (int f = 0; f < 75; ++f) slowed.update(dt, m_player, m_map, m_pathToPlayer);
        check(!slowed.isSlowed() && slowed.getSlowFactor() == 1.f, "Slow het han sau 2 giay: toc do tro lai binh thuong");
        slowed.applySlow(2.0f, 0.5f);
        for (int f = 0; f < 10; ++f) slowed.update(dt, m_player, m_map, m_pathToPlayer);
        slowed.applySlow(2.0f, 0.5f);
        check(slowed.getSlowTimer() > 1.7f && slowed.getSlowTimer() <= 2.0001f, "Bi lam cham lien tiep: chi lam moi thoi gian (khong vuot muc toi da)");
    }
    {
        Slime burn(m_assets, SlimeType::Green, m_map.findFreeSpotNear(origin + sf::Vector2f(600.f, 0.f), 20.f), unit);  // xa Player: khong bi tan cong
        const float hp0 = burn.getHp();
        burn.applyDot(3.0f, 0.5f, 2.f);
        check(burn.isBurning(), "DoT: quai dang bi bong");
        int tickEvents = 0, total = 0;
        for (int f = 0; f < 240; ++f) {  // 4 giay
            burn.update(dt, m_player, m_map, m_pathToPlayer);
            const int d = burn.consumeDotDamage();
            if (d > 0) { ++tickEvents; total += d; }
        }
        check(tickEvents >= 5 && tickEvents <= 6 && total == tickEvents * 2, "DoT: 3 giay / nhip 0.5 giay => " + std::to_string(tickEvents) + " nhip, moi nhip 2 sat thuong (KHONG phai moi frame)");
        check(!burn.isBurning() && std::fabs((hp0 - burn.getHp()) - static_cast<float>(total)) < 0.01f, "DoT het han dung luc; mau giam dung bang tong sat thuong cac nhip");

        Slime refreshed(m_assets, SlimeType::Green, m_map.findFreeSpotNear(origin + sf::Vector2f(600.f, 0.f), 20.f), unit);
        refreshed.applyDot(3.0f, 0.5f, 2.f);
        int ticks2 = 0, total2 = 0;
        for (int f = 0; f < 300; ++f) {
            if (f == 20 || f == 21 || f == 22 || f == 40) refreshed.applyDot(3.0f, 0.5f, 2.f);  // đánh liên tục để cố cộng dồn
            refreshed.update(dt, m_player, m_map, m_pathToPlayer);
            const int d = refreshed.consumeDotDamage();
            if (d > 0) { ++ticks2; total2 += d; }
        }
        check(ticks2 <= 7 && total2 == ticks2 * 2, "DoT bi danh lien tuc: chi lam moi thoi gian (" + std::to_string(ticks2) + " nhip, khong cong don sat thuong moi nhip)");

        balance::MonsterScale weak;
        weak.hp = 0.03f;  // máu ~1
        Slime victim(m_assets, SlimeType::Green, m_map.findFreeSpotNear(origin + sf::Vector2f(600.f, 0.f), 20.f), weak);
        victim.applyDot(2.0f, 0.5f, 5.f);
        for (int f = 0; f < 45; ++f) victim.update(dt, m_player, m_map, m_pathToPlayer);
        check(victim.isDead(), "DoT giet duoc quai");
        check(victim.consumeDeathReward() && !victim.consumeDeathReward(), "Quai chet vi DoT: nhan thuong DUNG MOT LAN (giong khi bi danh truc tiep)");
        Slime direct(m_assets, SlimeType::Green, m_map.findFreeSpotNear(origin + sf::Vector2f(600.f, 0.f), 20.f), weak);
        check(direct.takeDamage(999.f, origin) && direct.consumeDeathReward() && !direct.consumeDeathReward(), "Quai chet vi don truc tiep: nhan thuong dung mot lan");
        const sf::Vector2f deadPos = victim.getPosition();
        for (int f = 0; f < 120; ++f) victim.update(dt, m_player, m_map, m_pathToPlayer);
        check(!victim.consumeAttackHit() && util::distance(victim.getPosition(), deadPos) < 30.f, "Quai da chet khong tan cong va khong di chuyen");
    }

    // =======================================================================
    std::cout << "--- 5. Game Over, choi lai, tam dung, luu khi thoat ---\n";
    press(sf::Keyboard::Key::Enter);  // Victory -> Menu
    check(m_state == GameState::Menu && m_selectedLevel == kLevelCount, "Tu man Chien thang ve Menu, goi y choi man cuoi");
    for (int i = 0; i < 10; ++i) press(sf::Keyboard::Key::Left);
    check(m_selectedLevel == 1, "Menu: chon lai man 1 (da mo khoa tat ca)");
    press(sf::Keyboard::Key::Enter);
    check(m_state == GameState::Playing && m_levelId == 1, "Choi lai man cu");
    check(runLevel(1, false, false), "Choi lai man 1 sau khi thong quan");

    m_autopilotGod = false;
    m_autopilotPassive = true;
    startRun(3);
    long long before = onDisk().totalKills;
    for (int f = 0; f < 12000 && m_state != GameState::GameOver; ++f) step(f % 120 == 0);
    check(m_state == GameState::GameOver && m_player.isDead(), "Player het mau truoc khi toi cong -> Game Over");
    snapshot("G_gameover");
    check(onDisk().maxLevelUnlocked == kLevelCount + 1 && onDisk().playerMaxHp == expectedHp, "Game Over: khong nang cap them, khong mat tien trinh da luu");
    const int killsAtGameOver = m_kills;
    for (int f = 0; f < 120; ++f) step(false);
    check(m_player.isDead() && m_kills == killsAtGameOver && m_state == GameState::GameOver, "Game Over: khong the tiep tuc chien dau");
    (void)before;
    press(sf::Keyboard::Key::R);
    check(m_state == GameState::Playing && m_levelId == 3 && m_monsters.empty() && !m_player.isDead() && m_player.getHp() == m_player.getMaxHp() && m_score == 0,
          "Phim R: choi lai DUNG man vua thua, quai/diem duoc reset, day mau");

    m_autopilotPassive = false;
    m_autopilotGod = true;
    for (int f = 0; f < 6000 && m_unsavedKills < 2; ++f) step(false);
    check(m_unsavedKills >= 2, "Co quai bi ha chua duoc luu (de kiem tra luu khi thoat)");
    const int spawnedBefore = m_spawnedCount;
    const float timerBefore = m_spawnTimer;
    const float surviveBefore = m_survivalTime;
    press(sf::Keyboard::Key::Escape);
    check(m_state == GameState::Paused, "Esc: tam dung");
    for (int f = 0; f < 300; ++f) step(false);
    check(m_spawnedCount == spawnedBefore && m_spawnTimer == timerBefore && m_survivalTime == surviveBefore, "Tam dung: bo dem sinh quai va thoi gian choi dung yen");
    snapshot("P_paused");
    press(sf::Keyboard::Key::Escape);
    check(m_state == GameState::Playing, "Esc lan 2: tiep tuc");
    for (int f = 0; f < 600 && m_unsavedKills < 1; ++f) step(false);
    const int memTotal = m_progress.totalKills + m_unsavedKills;
    saveOnExit();  // đúng việc xảy ra khi người chơi đóng cửa sổ giữa lúc chơi
    check(onDisk().totalKills == memTotal && m_unsavedKills == 0, "Dong game giua luc choi: so quai ha duoc ghi vao CSV ngay, khong cong lap");
    saveOnExit();
    check(onDisk().totalKills == memTotal, "Luu lai nhieu lan khong lam tang tong quai");
    press(sf::Keyboard::Key::Escape);
    press(sf::Keyboard::Key::Q);
    check(m_state == GameState::Menu && onDisk().totalKills == memTotal, "Tam dung -> Q ve Menu: du lieu da duoc luu");

    // =======================================================================
    std::cout << "--- 6. Du lieu CSV hong / thieu / dang nhap lai ---\n";
    const ProgressRecord beforeLogout = onDisk();
    press(sf::Keyboard::Key::L);
    m_loginUser = "tester";
    m_loginPass = "pass1234";
    submitLoginForm();
    check(m_state == GameState::Menu && m_progress.maxLevelUnlocked == beforeLogout.maxLevelUnlocked && m_progress.playerMaxHp == beforeLogout.playerMaxHp &&
              m_progress.totalKills == beforeLogout.totalKills,
          "Dang nhap lai: khoi phuc dung man da mo khoa va chi so da luu");
    check(m_player.getBaseMaxHp() == expectedHp && m_player.getSlashTier() == balance::kMaxSlashTier, "Dang nhap lai: khoi phuc chi so Player va hieu ung chem");

    const int uid = m_user->userId;
    press(sf::Keyboard::Key::L);
    {
        std::ofstream out(m_repo.getDataDir() / "player_progress.csv", std::ios::trunc);
        out << "user_id,current_level,max_level_unlocked,player_max_hp,player_damage,total_kills,total_score,updated_at\n"
            << "rac rac rac\n" << uid << ",abc,9,-5,0,x,y,z\n" << (uid + 100) << ",2,3,130,24,5,50,2026-01-01 00:00:00\n";
        std::ofstream users(m_repo.getDataDir() / "users.csv", std::ios::app);
        users << "dong,hong,khong,hop,le\n";
    }
    m_loginUser = "tester";
    m_loginPass = "pass1234";
    submitLoginForm();
    check(m_state == GameState::Menu && m_progress.maxLevelUnlocked == 1 && m_progress.playerMaxHp == 100 && m_progress.playerDamage == 20,
          "File tien trinh HONG: game khong crash, dung du lieu mac dinh");
    press(sf::Keyboard::Key::Enter);
    check(m_state == GameState::Playing && m_levelId == 1, "Van choi duoc binh thuong sau khi gap file hong");
    saveOnExit();
    check(onDisk().playerMaxHp == 100 && disk.loadProgress(uid + 100).maxLevelUnlocked == 3, "Luu de tien trinh bi hong: dong cua nguoi khac van duoc giu nguyen");
    press(sf::Keyboard::Key::Escape);
    press(sf::Keyboard::Key::Q);
    press(sf::Keyboard::Key::L);
    {
        std::ofstream out(m_repo.getDataDir() / "player_progress.csv", std::ios::trunc);  // file RONG 0 byte
    }
    m_loginUser = "tester";
    m_loginPass = "pass1234";
    submitLoginForm();
    check(m_state == GameState::Menu && m_progress.maxLevelUnlocked == 1, "File tien trinh RONG: dung du lieu mac dinh, khong crash");
    fs::remove(m_repo.getDataDir() / "player_progress.csv");
    press(sf::Keyboard::Key::L);
    m_loginUser = "tester";
    m_loginPass = "pass1234";
    submitLoginForm();
    check(m_state == GameState::Menu, "File tien trinh BI XOA: dang nhap duoc, dung du lieu mac dinh");

    // ---- Chơi khách: chơi bình thường nhưng KHÔNG ghi bất cứ file nào ----
    auto slurp = [](const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };
    press(sf::Keyboard::Key::L);
    const std::string usersBefore = slurp(m_repo.getDataDir() / "users.csv");
    {
        UserRepository fresh(m_repo.getDataDir());  // tạo lại player_progress.csv vì bài kiểm tra trên đã xóa nó
        (void)fresh;
    }
    const std::string progressBefore = slurp(m_repo.getDataDir() / "player_progress.csv");
    press(sf::Keyboard::Key::F3);
    check(m_state == GameState::Menu && m_guest && !m_user.has_value() && m_progress.maxLevelUnlocked == 1, "F3: choi khach (tien trinh mac dinh, khong co tai khoan)");
    press(sf::Keyboard::Key::Enter);
    m_autopilotGod = true;
    for (int f = 0; f < 3000 && m_unsavedKills < 3; ++f) step(false);
    saveOnExit();
    press(sf::Keyboard::Key::Escape);
    press(sf::Keyboard::Key::Q);
    check(m_state == GameState::Menu && slurp(m_repo.getDataDir() / "users.csv") == usersBefore && slurp(m_repo.getDataDir() / "player_progress.csv") == progressBefore,
          "Choi khach: choi, luu, ve menu ma KHONG thay doi file CSV nao");

    // =======================================================================
    std::cout << "--- 7. Kiem tra bo cuc the Rogue-lite va Fullscreen ---\n";
    {
        // 1. Chế độ cửa sổ 1280x720
        const auto winLayout = getPowerUpCardLayout({1280.f, 720.f});
        const auto wCard0 = winLayout.getBounds(0);
        const auto wCard1 = winLayout.getBounds(1);
        const auto wCard2 = winLayout.getBounds(2);
        check(wCard0.position.x >= 0.f && wCard2.position.x + wCard2.size.x <= 1280.f,
              "Rogue-lite: 3 the nam tron ven trong man hinh 1280x720");
        check(winLayout.contains(0, {wCard0.position.x + 50.f, wCard0.position.y + 50.f}) &&
              winLayout.contains(1, {wCard1.position.x + 50.f, wCard1.position.y + 50.f}) &&
              winLayout.contains(2, {wCard2.position.x + 50.f, wCard2.position.y + 50.f}),
              "Rogue-lite (1280x720): Click dung the 1, the 2 va the 3");

        // 2. Chế độ Fullscreen 1080p (1920x1080)
        const auto fsLayout = getPowerUpCardLayout({1920.f, 1080.f});
        const auto fsCard0 = fsLayout.getBounds(0);
        const auto fsCard1 = fsLayout.getBounds(1);
        const auto fsCard2 = fsLayout.getBounds(2);
        check(fsCard0.position.x >= 0.f && fsCard2.position.x + fsCard2.size.x <= 1920.f,
              "Rogue-lite: 3 the can giua hop le trong Fullscreen 1080p (1920x1080)");
        check(fsLayout.contains(0, {fsCard0.position.x + 50.f, fsCard0.position.y + 50.f}),
              "Fullscreen: The 1 (trai) click duoc chinh xac");
        check(fsLayout.contains(1, {fsCard1.position.x + 50.f, fsCard1.position.y + 50.f}),
              "Fullscreen: The 2 (giua) click duoc chinh xac");
        check(fsLayout.contains(2, {fsCard2.position.x + 50.f, fsCard2.position.y + 50.f}),
              "Fullscreen: THE 3 (phai) click duoc hoan hao khong bi lech/khoa hitbox");
        check(!fsLayout.contains(0, {fsCard2.position.x + 50.f, fsCard2.position.y + 50.f}) &&
              !fsLayout.contains(1, {fsCard2.position.x + 50.f, fsCard2.position.y + 50.f}),
              "Fullscreen: Click the 3 KHONG bi nham sang the 1 hoac the 2");

        // 3. Chế độ 2K (2560x1440) và Laptop (1366x768)
        const auto k2Layout = getPowerUpCardLayout({2560.f, 1440.f});
        check(k2Layout.contains(2, {k2Layout.getBounds(2).position.x + 100.f, k2Layout.getBounds(2).position.y + 100.f}),
              "Rogue-lite: The 3 click thanh cong o do phan giai 2K (2560x1440)");
        const auto lapLayout = getPowerUpCardLayout({1366.f, 768.f});
        check(lapLayout.contains(2, {lapLayout.getBounds(2).position.x + 100.f, lapLayout.getBounds(2).position.y + 100.f}),
              "Rogue-lite: The 3 click thanh cong o do phan giai Laptop (1366x768)");
    }

    std::cout << "\n" << (checks - failures) << "/" << checks << " kiem tra dat\n";
    std::cout << (failures == 0 ? "SMOKE TEST: DAT\n" : "SMOKE TEST: KHONG DAT\n");
    return failures == 0 ? 0 : 1;
}
