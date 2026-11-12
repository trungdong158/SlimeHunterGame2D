#include "Shop.hpp"

#include <algorithm>

std::vector<LeaderboardEntry> ShopSystem::getTopScores(UserRepository& repo, int limit) {
    std::vector<LeaderboardEntry> list;
    const std::vector<UserRecord> users = repo.readAllUsers();

    for (const UserRecord& user : users) {
        const ProgressRecord prog = repo.loadProgress(user.userId);
        LeaderboardEntry entry;
        entry.username = user.username;
        entry.score = prog.totalScore;
        entry.kills = prog.totalKills;
        entry.maxLevel = prog.maxLevelUnlocked;
        list.push_back(entry);
    }

    std::sort(list.begin(), list.end(), [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
        return a.score > b.score;
    });

    if (list.size() > static_cast<std::size_t>(limit)) {
        list.resize(limit);
    }
    return list;
}

std::vector<ShopItem> ShopSystem::getShopItems(const ProgressRecord& progress) {
    std::vector<ShopItem> items;
    items.push_back({1, "Thể Lực Khởi Điểm", "+25 Máu tối đa vĩnh viễn", 200, progress.playerMaxHp >= 125});
    items.push_back({2, "Cổ Tay Thép", "+6 Sát thương cơ bản vĩnh viễn", 300, progress.playerDamage >= 26});
    items.push_back({3, "Vận Khí Rực Rỡ", "+15% Điểm số tích lũy", 400, false});
    return items;
}

bool ShopSystem::buyItem(int itemId, ProgressRecord& progress, UserRepository& repo) {
    if (itemId == 1 && progress.totalScore >= 200) {
        progress.totalScore -= 200;
        progress.playerMaxHp += 25;
        repo.saveProgress(progress);
        return true;
    } else if (itemId == 2 && progress.totalScore >= 300) {
        progress.totalScore -= 300;
        progress.playerDamage += 6;
        repo.saveProgress(progress);
        return true;
    } else if (itemId == 3 && progress.totalScore >= 400) {
        progress.totalScore -= 400;
        repo.saveProgress(progress);
        return true;
    }
    return false;
}
